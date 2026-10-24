#include "EarthOrientationDataParser.hpp"
#include "EphemerisEpochParsing.hpp"
#include "EphemerisTableMetadata.hpp"
#include "EphemerisTableParsing.hpp"
#include "EphemerisTimePolicy.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/CalendarTime.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kDataLabel = "Earth-orientation data";
constexpr std::string_view kRowLabel = "Earth-orientation";
constexpr std::string_view kValidityRangeId = "earth-orientation";
constexpr std::string_view kPredictionRangeId = "earth-orientation-prediction";
constexpr std::string_view kPredictionRangeDisplayName = "Earth-orientation prediction interval";

constexpr EphemerisTableParsing::TableParseSpec kParseSpec{
    .dataLabel = kDataLabel,
    .rowLabel = kRowLabel,
    .validityRangeId = kValidityRangeId,
    .rejectDuplicateDates = false,
    .endValidityRangeAtExpiration = false,
};

[[nodiscard]] bool startsWithIntegerYear(const std::string_view line) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitAsciiWhitespace(line);
    return !columns.empty() && NumericParsingUtilities::parseInt(columns.front()).has_value();
}

[[nodiscard]] std::optional<std::string> setPredictionEndpoint(
    IEarthOrientationProvider::DataInfo& info,
    skygate::core::AstronomicalEpoch EphemerisDateRange::* const endpoint,
    const std::string_view subject,
    const std::string& value,
    const std::size_t lineNumber,
    bool& hasEndpoint
)
{
    return EphemerisTableParsing::assignOptionalRangeEndpoint(
        info.predictionRange, [endpoint, subject, &value, lineNumber, &hasEndpoint](EphemerisDateRange& range) {
            return EphemerisTableMetadata::assignRangeEndpoint(
                range,
                endpoint,
                value,
                kPredictionRangeId,
                kPredictionRangeDisplayName,
                kDataLabel,
                subject,
                lineNumber,
                hasEndpoint
            );
        }
    );
}

void applyMetadataLine(
    IEarthOrientationProvider::DataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasPredictionStart,
    bool& hasPredictionEnd,
    std::optional<std::string>& diagnosticText
)
{
    if (EphemerisTableMetadata::applyCommonLine(info, line, kDataLabel, lineNumber, diagnosticText)) {
        return;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "prediction_start");
        value.has_value()) {
        diagnosticText = setPredictionEndpoint(
            info, &EphemerisDateRange::start, "prediction start", *value, lineNumber, hasPredictionStart
        );
        return;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "prediction_end");
        value.has_value()) {
        diagnosticText = setPredictionEndpoint(
            info, &EphemerisDateRange::end, "prediction end", *value, lineNumber, hasPredictionEnd
        );
        return;
    }
    }
}

// FINALS dates use two-digit years, so the MJD column is needed to pick the
// century. The calendar year, month, and day columns stay authoritative for
// the row date; the MJD only marks which side of the pivot the row falls on.
[[nodiscard]] std::optional<int> finalsYearFromMjd(const int twoDigitYear, const double mjd) noexcept
{
    if (twoDigitYear < 0 || twoDigitYear > 99 || !std::isfinite(mjd)) {
        return std::nullopt;
    }

    return (mjd <= EphemerisTimePolicy::kTwoDigitYearPivotMjd ? 1900 : 2000) + twoDigitYear;
}

[[nodiscard]] bool assignFinals2000AEntry(
    const int twoDigitYear,
    const int month,
    const int day,
    const double mjd,
    const char polarMotionFlag,
    const char ut1Flag,
    const double polarMotionXArcseconds,
    const double polarMotionYArcseconds,
    const double ut1MinusUtcSeconds,
    IEarthOrientationProvider::TableEntry& entry
) noexcept
{
    const std::optional<int> year = finalsYearFromMjd(twoDigitYear, mjd);
    if (!year.has_value()) {
        return false;
    }
    if ((polarMotionFlag != 'I' && polarMotionFlag != 'P') || (ut1Flag != 'I' && ut1Flag != 'P')) {
        return false;
    }

    const skygate::core::CivilDateTimeResult effectiveDate =
        skygate::core::CalendarTime::civilDateFromUtcParts(*year, month, day);
    if (!effectiveDate.has_value()) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromCivilDate(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;
    entry.predicted = polarMotionFlag == 'P' || ut1Flag == 'P';
    entry.estimated = false;
    return true;
}

// The FINALS 2000A rows address the same fields in two layouts: a whitespace
// table and the original fixed-width file. The mappings below only locate a
// field; parsing and assignment are shared by both layouts.
struct Finals2000AColumns {
    std::string_view twoDigitYear;
    std::string_view month;
    std::string_view day;
    std::string_view mjd;
    std::string_view polarMotionFlag;
    std::string_view polarMotionX;
    std::string_view polarMotionY;
    std::string_view ut1Flag;
    std::string_view ut1MinusUtc;
};

struct Finals2000ADate {
    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
};

[[nodiscard]] std::string_view
whitespaceColumn(const std::vector<std::string_view>& columns, const std::size_t index) noexcept
{
    return index < columns.size() ? columns[index] : std::string_view{};
}

[[nodiscard]] Finals2000AColumns finals2000AWhitespaceColumns(const std::vector<std::string_view>& columns) noexcept
{
    return Finals2000AColumns{
        .twoDigitYear = whitespaceColumn(columns, 0U),
        .month = whitespaceColumn(columns, 1U),
        .day = whitespaceColumn(columns, 2U),
        .mjd = whitespaceColumn(columns, 3U),
        .polarMotionFlag = whitespaceColumn(columns, 4U),
        .polarMotionX = whitespaceColumn(columns, 5U),
        .polarMotionY = whitespaceColumn(columns, 7U),
        .ut1Flag = whitespaceColumn(columns, 9U),
        .ut1MinusUtc = whitespaceColumn(columns, 10U),
    };
}

[[nodiscard]] Finals2000AColumns finals2000AFixedWidthColumns(const std::string_view line) noexcept
{
    return Finals2000AColumns{
        .twoDigitYear = TextParsingUtilities::fixedColumn(line, 0U, 2U),
        .month = TextParsingUtilities::fixedColumn(line, 2U, 2U),
        .day = TextParsingUtilities::fixedColumn(line, 4U, 2U),
        .mjd = TextParsingUtilities::fixedColumn(line, 7U, 8U),
        .polarMotionFlag = TextParsingUtilities::fixedColumn(line, 16U, 1U),
        .polarMotionX = TextParsingUtilities::fixedColumn(line, 18U, 9U),
        .polarMotionY = TextParsingUtilities::fixedColumn(line, 37U, 9U),
        .ut1Flag = TextParsingUtilities::fixedColumn(line, 57U, 1U),
        .ut1MinusUtc = TextParsingUtilities::fixedColumn(line, 58U, 10U),
    };
}

[[nodiscard]] std::optional<Finals2000ADate> finals2000ADate(const Finals2000AColumns& columns) noexcept
{
    const std::optional<int> twoDigitYear = NumericParsingUtilities::parseInt(columns.twoDigitYear);
    const std::optional<int> month = NumericParsingUtilities::parseInt(columns.month);
    const std::optional<int> day = NumericParsingUtilities::parseInt(columns.day);
    const std::optional<double> mjd = NumericParsingUtilities::parseFiniteDouble(columns.mjd);
    if (!twoDigitYear.has_value() || !month.has_value() || !day.has_value() || !mjd.has_value()
        || !finalsYearFromMjd(*twoDigitYear, *mjd).has_value()) {
        return std::nullopt;
    }

    return Finals2000ADate{
        .twoDigitYear = *twoDigitYear,
        .month = *month,
        .day = *day,
        .mjd = *mjd,
    };
}

[[nodiscard]] bool
parseFinals2000AColumns(const Finals2000AColumns& columns, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    const std::optional<Finals2000ADate> date = finals2000ADate(columns);
    if (!date.has_value() || columns.polarMotionFlag.size() != 1U || columns.ut1Flag.size() != 1U) {
        return false;
    }

    const std::optional<double> polarMotionXArcseconds =
        NumericParsingUtilities::parseFiniteDouble(columns.polarMotionX);
    const std::optional<double> polarMotionYArcseconds =
        NumericParsingUtilities::parseFiniteDouble(columns.polarMotionY);
    const std::optional<double> ut1MinusUtcSeconds = NumericParsingUtilities::parseFiniteDouble(columns.ut1MinusUtc);
    if (!polarMotionXArcseconds.has_value() || !polarMotionYArcseconds.has_value() || !ut1MinusUtcSeconds.has_value()) {
        return false;
    }

    return assignFinals2000AEntry(
        date->twoDigitYear,
        date->month,
        date->day,
        date->mjd,
        columns.polarMotionFlag.front(),
        columns.ut1Flag.front(),
        *polarMotionXArcseconds,
        *polarMotionYArcseconds,
        *ut1MinusUtcSeconds,
        entry
    );
}

[[nodiscard]] bool parseFinals2000AWhitespaceEntryLine(
    const std::vector<std::string_view>& columns, IEarthOrientationProvider::TableEntry& entry
) noexcept
{
    if (columns.size() < 11U) {
        return false;
    }

    return parseFinals2000AColumns(finals2000AWhitespaceColumns(columns), entry);
}

[[nodiscard]] bool
parseFinals2000AEntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    if (line.find(',') != std::string_view::npos) {
        return false;
    }
    if (parseFinals2000AWhitespaceEntryLine(TextParsingUtilities::splitAsciiWhitespace(line), entry)) {
        return true;
    }

    return parseFinals2000AColumns(finals2000AFixedWidthColumns(line), entry);
}

[[nodiscard]] bool parseIersC04EntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitAsciiWhitespace(line);
    if (columns.size() < 7U) {
        return false;
    }

    const std::optional<int> year = NumericParsingUtilities::parseInt(columns[0]);
    const std::optional<int> month = NumericParsingUtilities::parseInt(columns[1]);
    const std::optional<int> day = NumericParsingUtilities::parseInt(columns[2]);
    const std::optional<double> polarMotionXArcseconds = NumericParsingUtilities::parseFiniteDouble(columns[4]);
    const std::optional<double> polarMotionYArcseconds = NumericParsingUtilities::parseFiniteDouble(columns[5]);
    const std::optional<double> ut1MinusUtcSeconds = NumericParsingUtilities::parseFiniteDouble(columns[6]);
    if (!year.has_value() || !month.has_value() || !day.has_value() || !polarMotionXArcseconds.has_value()
        || !polarMotionYArcseconds.has_value() || !ut1MinusUtcSeconds.has_value()) {
        return false;
    }

    const skygate::core::CivilDateTimeResult effectiveDate =
        skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, *day);
    if (!effectiveDate.has_value()) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromCivilDate(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = *ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = *polarMotionXArcseconds;
    entry.polarMotionYArcseconds = *polarMotionYArcseconds;
    entry.predicted = false;
    entry.estimated = false;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    if (parseFinals2000AEntryLine(line, entry)) {
        return true;
    }

    const std::vector<std::string_view> columns = TextParsingUtilities::splitCommaSeparated(line);
    if (columns.size() < 4U || columns.size() > 6U) {
        return parseIersC04EntryLine(line, entry);
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromDateColumn(columns[0]);
    const std::optional<double> ut1MinusUtcSeconds = NumericParsingUtilities::parseFiniteDouble(columns[1]);
    const std::optional<double> polarMotionXArcseconds = NumericParsingUtilities::parseFiniteDouble(columns[2]);
    const std::optional<double> polarMotionYArcseconds = NumericParsingUtilities::parseFiniteDouble(columns[3]);
    bool predicted = false;
    bool estimated = false;
    if (!effectiveEpoch.has_value() || !ut1MinusUtcSeconds.has_value() || !polarMotionXArcseconds.has_value()
        || !polarMotionYArcseconds.has_value()) {
        return parseIersC04EntryLine(line, entry);
    }
    if (columns.size() == 5U && !TextParsingUtilities::parseBool(columns[4], predicted)) {
        return false;
    }
    if (columns.size() == 6U
        && (!TextParsingUtilities::parseBool(columns[4], predicted)
            || !TextParsingUtilities::parseBool(columns[5], estimated))) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = *ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = *polarMotionXArcseconds;
    entry.polarMotionYArcseconds = *polarMotionYArcseconds;
    entry.predicted = predicted;
    entry.estimated = estimated;
    return true;
}

[[nodiscard]] bool isFinals2000ADateOnlyLine(const std::string_view line) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitAsciiWhitespace(line);
    if (columns.size() == 4U) {
        return finals2000ADate(finals2000AWhitespaceColumns(columns)).has_value();
    }

    const Finals2000AColumns fixedWidthColumns = finals2000AFixedWidthColumns(line);
    if (!finals2000ADate(fixedWidthColumns).has_value()) {
        return false;
    }

    return fixedWidthColumns.polarMotionFlag.empty() && fixedWidthColumns.polarMotionX.empty()
           && fixedWidthColumns.polarMotionY.empty() && fixedWidthColumns.ut1Flag.empty()
           && fixedWidthColumns.ut1MinusUtc.empty();
}

[[nodiscard]] bool isIgnorableHeaderLine(const std::string_view line) noexcept
{
    return line.find(',') == std::string_view::npos && !startsWithIntegerYear(line);
}

// A prediction range is usable only when its metadata names both endpoints and
// forms a valid range.
[[nodiscard]] std::optional<std::string> checkPredictionRange(
    const IEarthOrientationProvider::DataInfo& info, const bool hasPredictionStart, const bool hasPredictionEnd
)
{
    if (!info.predictionRange.has_value()) {
        return std::nullopt;
    }
    if (!hasPredictionStart || !hasPredictionEnd || !info.predictionRange->isValid()) {
        return "Earth-orientation prediction metadata must include a valid start and end range.";
    }

    return std::nullopt;
}

}  // namespace

EarthOrientationDataParser::Result EarthOrientationDataParser::parse(const EphemerisTextDataAsset& asset)
{
    Result result;
    std::vector<IEarthOrientationProvider::TableEntry> entries;
    bool hasPredictionStart = false;
    bool hasPredictionEnd = false;
    if (std::optional<std::string> diagnosticText = EphemerisTableParsing::parsePayload(
            asset,
            result.dataInfo,
            entries,
            [&result, &hasPredictionStart, &hasPredictionEnd](
                const std::string_view line,
                const std::size_t lineNumber,
                std::optional<std::string>& metadataDiagnostic
            ) {
                applyMetadataLine(
                    result.dataInfo, line, lineNumber, hasPredictionStart, hasPredictionEnd, metadataDiagnostic
                );
            },
            [](const std::string_view rawLine, const std::string_view trimmedLine, const bool entriesEmpty) noexcept {
                return isFinals2000ADateOnlyLine(rawLine) || (entriesEmpty && isIgnorableHeaderLine(trimmedLine));
            },
            [](const std::string_view rawLine,
               const std::string_view,
               IEarthOrientationProvider::TableEntry& entry) noexcept { return parseEntryLine(rawLine, entry); },
            kParseSpec,
            [&hasPredictionStart, &hasPredictionEnd](const IEarthOrientationProvider::DataInfo& info) {
                return checkPredictionRange(info, hasPredictionStart, hasPredictionEnd);
            }
        );
        diagnosticText.has_value()) {
        EphemerisTableParsing::markFailed(result.dataInfo, std::move(*diagnosticText));
        return result;
    }

    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
