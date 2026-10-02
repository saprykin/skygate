#include "EarthOrientationDataParser.hpp"
#include "HighPrecisionTextParser.hpp"
#include "IEphemerisDataSnapshot.hpp"
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

constexpr std::string_view kValidityRangeId = "earth-orientation";
constexpr std::string_view kValidityRangeDisplayName = "Earth-orientation data";
constexpr std::string_view kPredictionRangeId = "earth-orientation-prediction";
constexpr std::string_view kPredictionRangeDisplayName = "Earth-orientation prediction interval";

[[nodiscard]] const HighPrecisionTextParser& textParser() noexcept
{
    static const HighPrecisionTextParser parser;
    return parser;
}

[[nodiscard]] bool startsWithIntegerYear(const std::string_view line) noexcept
{
    const std::vector<std::string_view> columns = textParser().splitAsciiWhitespace(line);
    int year = 0;
    return !columns.empty() && textParser().parseInt(columns.front(), year);
}

[[nodiscard]] EphemerisDateRange makeRange(
    const std::string_view id,
    const std::string_view displayName,
    const skygate::core::AstronomicalEpoch& start,
    const skygate::core::AstronomicalEpoch& end
)
{
    EphemerisDateRange range;
    range.id = id;
    range.displayName = displayName;
    range.start = start;
    range.end = end;
    return range;
}

[[nodiscard]] std::optional<std::string> setPredictionStart(
    IEarthOrientationProvider::DataInfo& info,
    const std::string& value,
    const std::size_t lineNumber,
    bool& hasPredictionStart
)
{
    const std::optional<skygate::core::CivilDateTime> date = textParser().parseUtcDate(value);
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Earth-orientation data contains malformed prediction start metadata at line "
               + std::to_string(lineNumber) + ".";
    }

    EphemerisDateRange range = info.predictionRange.value_or(EphemerisDateRange{});
    range.id = kPredictionRangeId;
    range.displayName = kPredictionRangeDisplayName;
    range.start = *epoch;
    info.predictionRange = range;
    hasPredictionStart = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> setPredictionEnd(
    IEarthOrientationProvider::DataInfo& info,
    const std::string& value,
    const std::size_t lineNumber,
    bool& hasPredictionEnd
)
{
    const std::optional<skygate::core::CivilDateTime> date = textParser().parseUtcDate(value);
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Earth-orientation data contains malformed prediction end metadata at line " + std::to_string(lineNumber)
               + ".";
    }

    EphemerisDateRange range = info.predictionRange.value_or(EphemerisDateRange{});
    range.id = kPredictionRangeId;
    range.displayName = kPredictionRangeDisplayName;
    range.end = *epoch;
    info.predictionRange = range;
    hasPredictionEnd = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> applyMetadataLine(
    IEarthOrientationProvider::DataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasPredictionStart,
    bool& hasPredictionEnd
)
{
    if (std::optional<std::string> value = textParser().metadataValue(line, "version"); value.has_value()) {
        if (!value->empty()) {
            info.version = std::move(*value);
        }
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "source"); value.has_value()) {
        if (!value->empty()) {
            info.provenance = std::move(*value);
        }
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "expires"); value.has_value()) {
        const std::optional<skygate::core::CivilDateTime> expiresDate = textParser().parseUtcDate(*value);
        const std::optional<skygate::core::AstronomicalEpoch> expiresEpoch =
            expiresDate.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*expiresDate)
                                    : std::nullopt;
        if (!expiresEpoch.has_value()) {
            return "Earth-orientation data contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        info.expiresAt = *expiresEpoch;
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "prediction_start"); value.has_value()) {
        return setPredictionStart(info, *value, lineNumber, hasPredictionStart);
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "prediction_end"); value.has_value()) {
        return setPredictionEnd(info, *value, lineNumber, hasPredictionEnd);
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<int> finalsYearFromMjd(const int twoDigitYear, const double mjd) noexcept
{
    if (twoDigitYear < 0 || twoDigitYear > 99 || !std::isfinite(mjd)) {
        return std::nullopt;
    }

    return (mjd <= 51543.0 ? 1900 : 2000) + twoDigitYear;
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

    skygate::core::CivilDateTime effectiveDate;
    effectiveDate.astronomicalYear = *year;
    effectiveDate.month = month;
    effectiveDate.day = day;
    effectiveDate.timeScale = skygate::core::TimeScale::Utc;
    if (!skygate::core::CalendarTime::isValidCivilDateTime(effectiveDate)) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(effectiveDate);
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

[[nodiscard]] bool parseFinals2000AWhitespaceEntryLine(
    const std::vector<std::string_view>& columns, IEarthOrientationProvider::TableEntry& entry
) noexcept
{
    if (columns.size() < 11U || (columns[4] != "I" && columns[4] != "P") || (columns[9] != "I" && columns[9] != "P")) {
        return false;
    }

    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    if (!textParser().parseInt(columns[0], twoDigitYear) || !textParser().parseInt(columns[1], month)
        || !textParser().parseInt(columns[2], day) || !textParser().parseFiniteDouble(columns[3], mjd)
        || !textParser().parseFiniteDouble(columns[5], polarMotionXArcseconds)
        || !textParser().parseFiniteDouble(columns[7], polarMotionYArcseconds)
        || !textParser().parseFiniteDouble(columns[10], ut1MinusUtcSeconds)) {
        return false;
    }

    return assignFinals2000AEntry(
        twoDigitYear,
        month,
        day,
        mjd,
        columns[4].front(),
        columns[9].front(),
        polarMotionXArcseconds,
        polarMotionYArcseconds,
        ut1MinusUtcSeconds,
        entry
    );
}

[[nodiscard]] bool
parseFinals2000AEntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    if (line.find(',') != std::string_view::npos) {
        return false;
    }
    if (parseFinals2000AWhitespaceEntryLine(textParser().splitAsciiWhitespace(line), entry)) {
        return true;
    }

    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
    if (!textParser().parseInt(textParser().fixedColumn(line, 0U, 2U), twoDigitYear)
        || !textParser().parseInt(textParser().fixedColumn(line, 2U, 2U), month)
        || !textParser().parseInt(textParser().fixedColumn(line, 4U, 2U), day)
        || !textParser().parseFiniteDouble(textParser().fixedColumn(line, 7U, 8U), mjd)) {
        return false;
    }

    const char polarMotionFlag = line.size() > 16U ? line[16U] : '\0';
    const char ut1Flag = line.size() > 57U ? line[57U] : '\0';
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    if (!textParser().parseFiniteDouble(textParser().fixedColumn(line, 18U, 9U), polarMotionXArcseconds)
        || !textParser().parseFiniteDouble(textParser().fixedColumn(line, 37U, 9U), polarMotionYArcseconds)
        || !textParser().parseFiniteDouble(textParser().fixedColumn(line, 58U, 10U), ut1MinusUtcSeconds)) {
        return false;
    }

    return assignFinals2000AEntry(
        twoDigitYear,
        month,
        day,
        mjd,
        polarMotionFlag,
        ut1Flag,
        polarMotionXArcseconds,
        polarMotionYArcseconds,
        ut1MinusUtcSeconds,
        entry
    );
}

[[nodiscard]] bool parseIersC04EntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = textParser().splitAsciiWhitespace(line);
    if (columns.size() < 7U) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    if (!textParser().parseInt(columns[0], year) || !textParser().parseInt(columns[1], month)
        || !textParser().parseInt(columns[2], day)
        || !textParser().parseFiniteDouble(columns[4], polarMotionXArcseconds)
        || !textParser().parseFiniteDouble(columns[5], polarMotionYArcseconds)
        || !textParser().parseFiniteDouble(columns[6], ut1MinusUtcSeconds)) {
        return false;
    }

    skygate::core::CivilDateTime effectiveDate;
    effectiveDate.astronomicalYear = year;
    effectiveDate.month = month;
    effectiveDate.day = day;
    effectiveDate.timeScale = skygate::core::TimeScale::Utc;
    if (!skygate::core::CalendarTime::isValidCivilDateTime(effectiveDate)) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;
    entry.predicted = false;
    entry.estimated = false;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, IEarthOrientationProvider::TableEntry& entry) noexcept
{
    if (parseFinals2000AEntryLine(line, entry)) {
        return true;
    }

    const std::vector<std::string_view> columns = textParser().splitCommaSeparated(line);
    if (columns.size() < 4U || columns.size() > 6U) {
        return parseIersC04EntryLine(line, entry);
    }

    const std::optional<skygate::core::CivilDateTime> effectiveDate = textParser().parseUtcDate(columns[0]);
    double ut1MinusUtcSeconds = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    bool predicted = false;
    bool estimated = false;
    if (!effectiveDate.has_value() || !textParser().parseFiniteDouble(columns[1], ut1MinusUtcSeconds)
        || !textParser().parseFiniteDouble(columns[2], polarMotionXArcseconds)
        || !textParser().parseFiniteDouble(columns[3], polarMotionYArcseconds)) {
        return parseIersC04EntryLine(line, entry);
    }
    if (columns.size() == 5U && !textParser().parseBool(columns[4], predicted)) {
        return false;
    }
    if (columns.size() == 6U
        && (!textParser().parseBool(columns[4], predicted) || !textParser().parseBool(columns[5], estimated))) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;
    entry.predicted = predicted;
    entry.estimated = estimated;
    return true;
}

[[nodiscard]] bool isFinals2000ADateOnlyLine(const std::string_view line) noexcept
{
    const std::vector<std::string_view> columns = textParser().splitAsciiWhitespace(line);
    if (columns.size() == 4U) {
        int twoDigitYear = 0;
        int month = 0;
        int day = 0;
        double mjd = 0.0;
        return textParser().parseInt(columns[0], twoDigitYear) && textParser().parseInt(columns[1], month)
               && textParser().parseInt(columns[2], day) && textParser().parseFiniteDouble(columns[3], mjd)
               && finalsYearFromMjd(twoDigitYear, mjd).has_value();
    }

    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
    if (!textParser().parseInt(textParser().fixedColumn(line, 0U, 2U), twoDigitYear)
        || !textParser().parseInt(textParser().fixedColumn(line, 2U, 2U), month)
        || !textParser().parseInt(textParser().fixedColumn(line, 4U, 2U), day)
        || !textParser().parseFiniteDouble(textParser().fixedColumn(line, 7U, 8U), mjd)
        || !finalsYearFromMjd(twoDigitYear, mjd).has_value()) {
        return false;
    }

    return textParser().fixedColumn(line, 16U, 1U).empty() && textParser().fixedColumn(line, 18U, 9U).empty()
           && textParser().fixedColumn(line, 37U, 9U).empty() && textParser().fixedColumn(line, 57U, 1U).empty()
           && textParser().fixedColumn(line, 58U, 10U).empty();
}

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return textParser().startsWith(line, "effective_utc_date");
}

[[nodiscard]] bool isIgnorableHeaderLine(const std::string_view line) noexcept
{
    return line.find(',') == std::string_view::npos && !startsWithIntegerYear(line);
}

[[nodiscard]] EarthOrientationDataParser::Result failureResult(
    IEarthOrientationProvider::DataInfo info,
    const IEarthOrientationProvider::DataStatus status,
    std::string diagnosticText
)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    EarthOrientationDataParser::Result result;
    result.dataInfo = std::move(info);
    return result;
}

[[nodiscard]] bool isValidPredictionRange(const EphemerisDateRange& range) noexcept
{
    return range.start.isFiniteUtc() && range.end.isFiniteUtc() && range.start.sortKey() <= range.end.sortKey();
}

}  // namespace

EarthOrientationDataParser::Result EarthOrientationDataParser::parse(const EphemerisTextDataAsset& asset)
{
    IEarthOrientationProvider::DataInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<IEarthOrientationProvider::TableEntry> entries;
    bool hasPredictionStart = false;
    bool hasPredictionEnd = false;
    std::string_view remaining = asset.content;
    std::size_t lineNumber = 0U;
    while (!remaining.empty()) {
        ++lineNumber;
        std::string_view line = textParser().takeLine(remaining);

        const std::string_view trimmedLine = textParser().trimAsciiWhitespace(line);
        if (trimmedLine.empty()) {
            continue;
        }
        if (textParser().startsWith(trimmedLine, "#@ ")) {
            if (std::optional<std::string> diagnosticText =
                    applyMetadataLine(info, trimmedLine, lineNumber, hasPredictionStart, hasPredictionEnd);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), IEarthOrientationProvider::DataStatus::Malformed, std::move(*diagnosticText)
                );
            }
            continue;
        }
        if (textParser().startsWith(trimmedLine, "#")) {
            continue;
        }
        if (isFinals2000ADateOnlyLine(line)) {
            continue;
        }
        if (isHeaderLine(trimmedLine) || (entries.empty() && isIgnorableHeaderLine(trimmedLine))) {
            continue;
        }

        IEarthOrientationProvider::TableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                IEarthOrientationProvider::DataStatus::Malformed,
                "Earth-orientation data contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(
            std::move(info),
            IEarthOrientationProvider::DataStatus::Malformed,
            "Earth-orientation data contains no rows."
        );
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const IEarthOrientationProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });
    if (!sorted) {
        return failureResult(
            std::move(info),
            IEarthOrientationProvider::DataStatus::Malformed,
            "Earth-orientation rows are not sorted by UTC date."
        );
    }

    if (info.predictionRange.has_value()) {
        EphemerisDateRange predictionRange = *info.predictionRange;
        if (!hasPredictionStart || !hasPredictionEnd || !isValidPredictionRange(predictionRange)) {
            return failureResult(
                std::move(info),
                IEarthOrientationProvider::DataStatus::Malformed,
                "Earth-orientation prediction metadata must include a valid start and end range."
            );
        }
        info.predictionRange = std::move(predictionRange);
    }

    info.validityRange = makeRange(
        kValidityRangeId, kValidityRangeDisplayName, entries.front().effectiveUtcEpoch, entries.back().effectiveUtcEpoch
    );
    info.status = IEarthOrientationProvider::DataStatus::Available;
    info.diagnosticText = "Earth-orientation data loaded.";
    EarthOrientationDataParser::Result result;
    result.dataInfo = std::move(info);
    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
