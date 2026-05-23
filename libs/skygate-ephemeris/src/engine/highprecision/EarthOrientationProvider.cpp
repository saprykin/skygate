#include "engine/highprecision/EarthOrientationProvider.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
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

[[nodiscard]] std::string_view trimAsciiWhitespace(std::string_view text) noexcept
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1U);
    }
    return text;
}

[[nodiscard]] bool startsWith(std::string_view text, const std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0U, prefix.size()) == prefix;
}

[[nodiscard]] bool parseInt(const std::string_view text, int& value) noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return false;
    }

    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool parseDouble(const std::string_view text, double& value) noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return false;
    }

    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && std::isfinite(value);
}

[[nodiscard]] std::vector<std::string_view> splitAsciiWhitespace(std::string_view text)
{
    std::vector<std::string_view> tokens;
    while (true) {
        text = trimAsciiWhitespace(text);
        if (text.empty()) {
            break;
        }

        std::size_t end = 0U;
        while (end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) == 0) {
            ++end;
        }
        tokens.push_back(text.substr(0U, end));
        text.remove_prefix(end);
    }
    return tokens;
}

[[nodiscard]] bool startsWithIntegerYear(const std::string_view line) noexcept
{
    const std::vector<std::string_view> columns = splitAsciiWhitespace(line);
    int year = 0;
    return !columns.empty() && parseInt(columns.front(), year);
}

[[nodiscard]] bool parseBool(const std::string_view text, bool& value) noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed == "true" || trimmed == "1") {
        value = true;
        return true;
    }
    if (trimmed == "false" || trimmed == "0") {
        value = false;
        return true;
    }

    return false;
}

[[nodiscard]] std::optional<CivilDateTime> parseUtcDate(std::string_view text) noexcept
{
    text = trimAsciiWhitespace(text);
    const std::size_t yearSearchStart = startsWith(text, "-") ? 1U : 0U;
    const std::size_t firstDash = text.find('-', yearSearchStart);
    const std::size_t secondDash =
        firstDash == std::string_view::npos ? std::string_view::npos : text.find('-', firstDash + 1U);
    if (firstDash == std::string_view::npos || secondDash == std::string_view::npos) {
        return std::nullopt;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseInt(text.substr(0U, firstDash), year)
        || !parseInt(text.substr(firstDash + 1U, secondDash - firstDash - 1U), month)
        || !parseInt(text.substr(secondDash + 1U), day)) {
        return std::nullopt;
    }

    CivilDateTime dateTime;
    dateTime.astronomicalYear = year;
    dateTime.month = month;
    dateTime.day = day;
    dateTime.timeScale = TimeScale::Utc;
    if (!isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }

    return dateTime;
}

[[nodiscard]] std::optional<AstronomicalEpoch> epochFromUtcDate(const CivilDateTime& dateTime) noexcept
{
    return astronomicalEpochFromCivilDateTime(dateTime);
}

[[nodiscard]] double epochSortKey(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalized = normalizedAstronomicalEpoch(epoch);
    return normalized.julianDatePart1 + normalized.julianDatePart2;
}

[[nodiscard]] bool isFiniteUtcEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.timeScale == TimeScale::Utc && std::isfinite(epoch.julianDatePart1)
           && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] std::optional<std::string> metadataValue(const std::string_view line, const std::string_view key)
{
    const std::string prefix = "#@ " + std::string(key);
    if (!startsWith(line, prefix)) {
        return std::nullopt;
    }
    if (line.size() > prefix.size() && std::isspace(static_cast<unsigned char>(line[prefix.size()])) == 0) {
        return std::nullopt;
    }

    return std::string(trimAsciiWhitespace(line.substr(prefix.size())));
}

[[nodiscard]] EphemerisDateRange makeRange(
    const std::string_view id,
    const std::string_view displayName,
    const AstronomicalEpoch& start,
    const AstronomicalEpoch& end
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
    EarthOrientationDataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasPredictionStart
)
{
    const std::optional<CivilDateTime> date = parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch = date.has_value() ? epochFromUtcDate(*date) : std::nullopt;
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
    EarthOrientationDataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasPredictionEnd
)
{
    const std::optional<CivilDateTime> date = parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch = date.has_value() ? epochFromUtcDate(*date) : std::nullopt;
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
    EarthOrientationDataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasPredictionStart,
    bool& hasPredictionEnd
)
{
    if (std::optional<std::string> value = metadataValue(line, "version"); value.has_value()) {
        if (!value->empty()) {
            info.version = std::move(*value);
        }
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "source"); value.has_value()) {
        if (!value->empty()) {
            info.provenance = std::move(*value);
        }
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "expires"); value.has_value()) {
        const std::optional<CivilDateTime> expiresDate = parseUtcDate(*value);
        const std::optional<AstronomicalEpoch> expiresEpoch =
            expiresDate.has_value() ? epochFromUtcDate(*expiresDate) : std::nullopt;
        if (!expiresEpoch.has_value()) {
            return "Earth-orientation data contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        info.expiresAt = *expiresEpoch;
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "prediction_start"); value.has_value()) {
        return setPredictionStart(info, *value, lineNumber, hasPredictionStart);
    }
    if (std::optional<std::string> value = metadataValue(line, "prediction_end"); value.has_value()) {
        return setPredictionEnd(info, *value, lineNumber, hasPredictionEnd);
    }

    return std::nullopt;
}

[[nodiscard]] std::vector<std::string_view> splitCsvLine(std::string_view line)
{
    std::vector<std::string_view> columns;
    while (true) {
        const std::size_t comma = line.find(',');
        columns.push_back(trimAsciiWhitespace(comma == std::string_view::npos ? line : line.substr(0U, comma)));
        if (comma == std::string_view::npos) {
            break;
        }
        line.remove_prefix(comma + 1U);
    }
    return columns;
}

[[nodiscard]] std::string_view
fixedColumn(const std::string_view line, const std::size_t offset, const std::size_t length) noexcept
{
    if (offset >= line.size()) {
        return {};
    }

    return trimAsciiWhitespace(line.substr(offset, std::min(length, line.size() - offset)));
}

[[nodiscard]] bool
isBlankFixedColumn(const std::string_view line, const std::size_t offset, const std::size_t length) noexcept
{
    if (offset >= line.size()) {
        return true;
    }

    return trimAsciiWhitespace(line.substr(offset, std::min(length, line.size() - offset))).empty();
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
    EarthOrientationTableEntry& entry
) noexcept
{
    const std::optional<int> year = finalsYearFromMjd(twoDigitYear, mjd);
    if (!year.has_value()) {
        return false;
    }
    if ((polarMotionFlag != 'I' && polarMotionFlag != 'P') || (ut1Flag != 'I' && ut1Flag != 'P')) {
        return false;
    }

    CivilDateTime effectiveDate;
    effectiveDate.astronomicalYear = *year;
    effectiveDate.month = month;
    effectiveDate.day = day;
    effectiveDate.timeScale = TimeScale::Utc;
    if (!isValidCivilDateTime(effectiveDate)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch = epochFromUtcDate(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = effectiveDate;
    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;
    entry.predicted = polarMotionFlag == 'P' || ut1Flag == 'P';
    entry.estimated = false;
    return true;
}

[[nodiscard]] bool parseFinals2000AWhitespaceEntryLine(
    const std::vector<std::string_view>& columns, EarthOrientationTableEntry& entry
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
    if (!parseInt(columns[0], twoDigitYear) || !parseInt(columns[1], month) || !parseInt(columns[2], day)
        || !parseDouble(columns[3], mjd) || !parseDouble(columns[5], polarMotionXArcseconds)
        || !parseDouble(columns[7], polarMotionYArcseconds) || !parseDouble(columns[10], ut1MinusUtcSeconds)) {
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

[[nodiscard]] bool parseFinals2000AEntryLine(std::string_view line, EarthOrientationTableEntry& entry) noexcept
{
    if (line.find(',') != std::string_view::npos) {
        return false;
    }
    if (parseFinals2000AWhitespaceEntryLine(splitAsciiWhitespace(line), entry)) {
        return true;
    }

    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
    if (!parseInt(fixedColumn(line, 0U, 2U), twoDigitYear) || !parseInt(fixedColumn(line, 2U, 2U), month)
        || !parseInt(fixedColumn(line, 4U, 2U), day) || !parseDouble(fixedColumn(line, 7U, 8U), mjd)) {
        return false;
    }

    const char polarMotionFlag = line.size() > 16U ? line[16U] : '\0';
    const char ut1Flag = line.size() > 57U ? line[57U] : '\0';
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    if (!parseDouble(fixedColumn(line, 18U, 9U), polarMotionXArcseconds)
        || !parseDouble(fixedColumn(line, 37U, 9U), polarMotionYArcseconds)
        || !parseDouble(fixedColumn(line, 58U, 10U), ut1MinusUtcSeconds)) {
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

[[nodiscard]] bool parseIersC04EntryLine(std::string_view line, EarthOrientationTableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = splitAsciiWhitespace(line);
    if (columns.size() < 7U) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    if (!parseInt(columns[0], year) || !parseInt(columns[1], month) || !parseInt(columns[2], day)
        || !parseDouble(columns[4], polarMotionXArcseconds) || !parseDouble(columns[5], polarMotionYArcseconds)
        || !parseDouble(columns[6], ut1MinusUtcSeconds)) {
        return false;
    }

    CivilDateTime effectiveDate;
    effectiveDate.astronomicalYear = year;
    effectiveDate.month = month;
    effectiveDate.day = day;
    effectiveDate.timeScale = TimeScale::Utc;
    if (!isValidCivilDateTime(effectiveDate)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch = epochFromUtcDate(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = effectiveDate;
    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;
    entry.predicted = false;
    entry.estimated = false;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, EarthOrientationTableEntry& entry) noexcept
{
    if (parseFinals2000AEntryLine(line, entry)) {
        return true;
    }

    const std::vector<std::string_view> columns = splitCsvLine(line);
    if (columns.size() < 4U || columns.size() > 6U) {
        return parseIersC04EntryLine(line, entry);
    }

    const std::optional<CivilDateTime> effectiveDate = parseUtcDate(columns[0]);
    double ut1MinusUtcSeconds = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    bool predicted = false;
    bool estimated = false;
    if (!effectiveDate.has_value() || !parseDouble(columns[1], ut1MinusUtcSeconds)
        || !parseDouble(columns[2], polarMotionXArcseconds) || !parseDouble(columns[3], polarMotionYArcseconds)) {
        return parseIersC04EntryLine(line, entry);
    }
    if (columns.size() == 5U && !parseBool(columns[4], predicted)) {
        return false;
    }
    if (columns.size() == 6U && (!parseBool(columns[4], predicted) || !parseBool(columns[5], estimated))) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch = epochFromUtcDate(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = *effectiveDate;
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
    const std::vector<std::string_view> columns = splitAsciiWhitespace(line);
    if (columns.size() == 4U) {
        int twoDigitYear = 0;
        int month = 0;
        int day = 0;
        double mjd = 0.0;
        return parseInt(columns[0], twoDigitYear) && parseInt(columns[1], month) && parseInt(columns[2], day)
               && parseDouble(columns[3], mjd) && finalsYearFromMjd(twoDigitYear, mjd).has_value();
    }

    int twoDigitYear = 0;
    int month = 0;
    int day = 0;
    double mjd = 0.0;
    if (!parseInt(fixedColumn(line, 0U, 2U), twoDigitYear) || !parseInt(fixedColumn(line, 2U, 2U), month)
        || !parseInt(fixedColumn(line, 4U, 2U), day) || !parseDouble(fixedColumn(line, 7U, 8U), mjd)
        || !finalsYearFromMjd(twoDigitYear, mjd).has_value()) {
        return false;
    }

    return isBlankFixedColumn(line, 16U, 1U) && isBlankFixedColumn(line, 18U, 9U) && isBlankFixedColumn(line, 37U, 9U)
           && isBlankFixedColumn(line, 57U, 1U) && isBlankFixedColumn(line, 58U, 10U);
}

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return startsWith(line, "effective_utc_date");
}

[[nodiscard]] bool isIgnorableHeaderLine(const std::string_view line) noexcept
{
    return line.find(',') == std::string_view::npos && !startsWithIntegerYear(line);
}

[[nodiscard]] EarthOrientationDataLoadResult
failureResult(EarthOrientationDataInfo info, const EarthOrientationDataStatus status, std::string diagnosticText)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    EarthOrientationDataLoadResult result;
    result.dataInfo = std::move(info);
    return result;
}

[[nodiscard]] bool isValidPredictionRange(const EphemerisDateRange& range) noexcept
{
    return isFiniteUtcEpoch(range.start) && isFiniteUtcEpoch(range.end)
           && epochSortKey(range.start) <= epochSortKey(range.end);
}

[[nodiscard]] bool epochInRange(const AstronomicalEpoch& epoch, const EphemerisDateRange& range) noexcept
{
    const double key = epochSortKey(epoch);
    return key >= epochSortKey(range.start) && key <= epochSortKey(range.end);
}

[[nodiscard]] bool epochAfter(const AstronomicalEpoch& epoch, const AstronomicalEpoch& boundary) noexcept
{
    return epochSortKey(epoch) > epochSortKey(boundary);
}

void addSampleWarning(EarthOrientationSample& sample, const EarthOrientationSampleWarningCode code) noexcept
{
    if (sample.status == EarthOrientationSampleStatus::Valid) {
        sample.status = EarthOrientationSampleStatus::Degraded;
    }
    sample.addWarning(code);
}

[[nodiscard]] EarthOrientationSample failedSample(
    const AstronomicalEpoch& utcEpoch, const EarthOrientationSampleWarningCode code, std::string diagnosticText
)
{
    EarthOrientationSample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.status = EarthOrientationSampleStatus::Failed;
    sample.diagnosticText = std::move(diagnosticText);
    sample.addWarning(code);
    return sample;
}

[[nodiscard]] EarthOrientationSample
missingDataSample(const AstronomicalEpoch& utcEpoch, const EarthOrientationSampleOptions& options)
{
    if (!options.allowMissingDataZeroFallback) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampleWarningCode::MissingData,
            "Earth-orientation data is unavailable for the requested epoch."
        );
    }

    EarthOrientationSample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.status = EarthOrientationSampleStatus::Degraded;
    sample.addWarning(EarthOrientationSampleWarningCode::MissingData);
    sample.diagnosticText = "Earth-orientation data used a degraded zero-value fallback.";
    return sample;
}

[[nodiscard]] EarthOrientationSample
sampleFromEntry(const AstronomicalEpoch& requestedEpoch, const EarthOrientationTableEntry& entry)
{
    EarthOrientationSample sample;
    sample.requestedUtcEpoch = requestedEpoch;
    sample.ut1MinusUtcSeconds = entry.ut1MinusUtcSeconds;
    sample.polarMotionXArcseconds = entry.polarMotionXArcseconds;
    sample.polarMotionYArcseconds = entry.polarMotionYArcseconds;
    sample.predicted = entry.predicted;
    sample.estimated = entry.estimated;
    sample.status = EarthOrientationSampleStatus::Valid;
    sample.diagnosticText = "Earth-orientation sample resolved.";
    return sample;
}

[[nodiscard]] EarthOrientationSample interpolateSamples(
    const AstronomicalEpoch& utcEpoch, const EarthOrientationTableEntry& lower, const EarthOrientationTableEntry& upper
)
{
    const double lowerKey = epochSortKey(lower.effectiveUtcEpoch);
    const double upperKey = epochSortKey(upper.effectiveUtcEpoch);
    const double denominator = upperKey - lowerKey;
    if (denominator <= 0.0) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampleWarningCode::InvalidInput,
            "Earth-orientation rows do not form a valid interpolation interval."
        );
    }

    const double ratio = (epochSortKey(utcEpoch) - lowerKey) / denominator;
    EarthOrientationSample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.ut1MinusUtcSeconds =
        lower.ut1MinusUtcSeconds + (upper.ut1MinusUtcSeconds - lower.ut1MinusUtcSeconds) * ratio;
    sample.polarMotionXArcseconds =
        lower.polarMotionXArcseconds + (upper.polarMotionXArcseconds - lower.polarMotionXArcseconds) * ratio;
    sample.polarMotionYArcseconds =
        lower.polarMotionYArcseconds + (upper.polarMotionYArcseconds - lower.polarMotionYArcseconds) * ratio;
    sample.predicted = lower.predicted || upper.predicted;
    sample.estimated = lower.estimated || upper.estimated;
    sample.status = EarthOrientationSampleStatus::Valid;
    sample.diagnosticText = "Earth-orientation sample interpolated.";
    return sample;
}

void applyDataWarnings(
    EarthOrientationSample& sample,
    const EarthOrientationDataInfo& info,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampleOptions& options
) noexcept
{
    if (info.status == EarthOrientationDataStatus::Stale
        || (info.expiresAt.has_value() && epochAfter(utcEpoch, *info.expiresAt))) {
        addSampleWarning(sample, EarthOrientationSampleWarningCode::StaleData);
    }

    if (info.status == EarthOrientationDataStatus::Estimated || sample.estimated) {
        sample.estimated = true;
        addSampleWarning(sample, EarthOrientationSampleWarningCode::EstimatedData);
    }

    if (sample.predicted || (info.predictionRange.has_value() && epochInRange(utcEpoch, *info.predictionRange))) {
        sample.predicted = true;
        if (options.degradePredictedData) {
            addSampleWarning(sample, EarthOrientationSampleWarningCode::PredictedData);
        } else {
            sample.addWarning(EarthOrientationSampleWarningCode::PredictedData);
        }
    }
}

}  // namespace

TableBackedEarthOrientationProvider::TableBackedEarthOrientationProvider(
    EarthOrientationDataInfo dataInfo, std::vector<EarthOrientationTableEntry> entries
)
    : m_dataInfo(std::move(dataInfo)), m_entries(std::move(entries))
{
}

const EarthOrientationDataInfo& TableBackedEarthOrientationProvider::dataInfo() const noexcept
{
    return m_dataInfo;
}

std::span<const EarthOrientationTableEntry> TableBackedEarthOrientationProvider::entries() const noexcept
{
    return m_entries;
}

EarthOrientationDataLoadResult loadEarthOrientationDataFromSnapshot(
    const IEphemerisDataSnapshot& snapshot, const EarthOrientationDataLoadOptions& options
)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.earthOrientationDataAsset();
    if (!asset.has_value()) {
        EarthOrientationDataInfo info;
        return failureResult(
            std::move(info),
            EarthOrientationDataStatus::Missing,
            "Earth-orientation data asset is missing from the data snapshot."
        );
    }

    return loadEarthOrientationDataFromTextAsset(*asset, options);
}

EarthOrientationDataLoadResult loadEarthOrientationDataFromTextAsset(
    const EphemerisTextDataAsset& asset, const EarthOrientationDataLoadOptions& options
)
{
    EarthOrientationDataInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<EarthOrientationTableEntry> entries;
    bool hasPredictionStart = false;
    bool hasPredictionEnd = false;
    std::string_view remaining = asset.content;
    std::size_t lineNumber = 0U;
    while (!remaining.empty()) {
        ++lineNumber;
        const std::size_t newline = remaining.find('\n');
        std::string_view line = newline == std::string_view::npos ? remaining : remaining.substr(0U, newline);
        remaining = newline == std::string_view::npos ? std::string_view{} : remaining.substr(newline + 1U);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        const std::string_view trimmedLine = trimAsciiWhitespace(line);
        if (trimmedLine.empty()) {
            continue;
        }
        if (startsWith(trimmedLine, "#@ ")) {
            if (std::optional<std::string> diagnosticText =
                    applyMetadataLine(info, trimmedLine, lineNumber, hasPredictionStart, hasPredictionEnd);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), EarthOrientationDataStatus::Malformed, std::move(*diagnosticText)
                );
            }
            continue;
        }
        if (startsWith(trimmedLine, "#")) {
            continue;
        }
        if (isFinals2000ADateOnlyLine(line)) {
            continue;
        }
        if (isHeaderLine(trimmedLine) || (entries.empty() && isIgnorableHeaderLine(trimmedLine))) {
            continue;
        }

        EarthOrientationTableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                EarthOrientationDataStatus::Malformed,
                "Earth-orientation data contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(
            std::move(info), EarthOrientationDataStatus::Malformed, "Earth-orientation data contains no rows."
        );
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const EarthOrientationTableEntry& entry) {
        return epochSortKey(entry.effectiveUtcEpoch);
    });
    if (!sorted) {
        return failureResult(
            std::move(info), EarthOrientationDataStatus::Malformed, "Earth-orientation rows are not sorted by UTC date."
        );
    }

    if (info.predictionRange.has_value()) {
        EphemerisDateRange predictionRange = *info.predictionRange;
        if (!hasPredictionStart || !hasPredictionEnd || !isValidPredictionRange(predictionRange)) {
            return failureResult(
                std::move(info),
                EarthOrientationDataStatus::Malformed,
                "Earth-orientation prediction metadata must include a valid start and end range."
            );
        }
        info.predictionRange = std::move(predictionRange);
    }

    info.validityRange = makeRange(
        kValidityRangeId, kValidityRangeDisplayName, entries.front().effectiveUtcEpoch, entries.back().effectiveUtcEpoch
    );
    info.status = EarthOrientationDataStatus::Available;
    info.diagnosticText = "Earth-orientation data loaded.";
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && isFiniteUtcEpoch(*options.referenceEpoch)
        && epochSortKey(*options.referenceEpoch) > epochSortKey(*info.expiresAt)) {
        info.status = EarthOrientationDataStatus::Stale;
        info.diagnosticText = "Earth-orientation data is stale for the reference epoch.";
    }

    EarthOrientationDataLoadResult result;
    result.dataInfo = info;
    result.provider = std::make_shared<TableBackedEarthOrientationProvider>(std::move(info), std::move(entries));
    return result;
}

EarthOrientationSample sampleEarthOrientation(
    const IEarthOrientationProvider* provider,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampleOptions& options
)
{
    if (!isFiniteUtcEpoch(utcEpoch)) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampleWarningCode::InvalidInput,
            "Earth-orientation sampling requires a finite UTC epoch."
        );
    }

    if (provider == nullptr || !provider->dataInfo().isUsable() || provider->entries().empty()) {
        return missingDataSample(utcEpoch, options);
    }

    const std::span<const EarthOrientationTableEntry> entries = provider->entries();
    const double requestedKey = epochSortKey(utcEpoch);
    const auto lowerBound = std::ranges::lower_bound(entries, requestedKey, {}, [](const auto& entry) {
        return epochSortKey(entry.effectiveUtcEpoch);
    });

    EarthOrientationSample sample;
    if (lowerBound != entries.end() && epochSortKey(lowerBound->effectiveUtcEpoch) == requestedKey) {
        sample = sampleFromEntry(utcEpoch, *lowerBound);
    } else if (lowerBound == entries.begin()) {
        if (!options.allowOutOfRangeNearestSampleFallback) {
            return failedSample(
                utcEpoch,
                EarthOrientationSampleWarningCode::EpochOutsideRange,
                "Requested epoch is before the first Earth-orientation row."
            );
        }
        sample = sampleFromEntry(utcEpoch, entries.front());
        addSampleWarning(sample, EarthOrientationSampleWarningCode::EpochOutsideRange);
        sample.diagnosticText = "Earth-orientation sample used the first available row as a degraded fallback.";
    } else if (lowerBound == entries.end()) {
        if (!options.allowOutOfRangeNearestSampleFallback) {
            return failedSample(
                utcEpoch,
                EarthOrientationSampleWarningCode::EpochOutsideRange,
                "Requested epoch is after the last Earth-orientation row."
            );
        }
        sample = sampleFromEntry(utcEpoch, entries.back());
        addSampleWarning(sample, EarthOrientationSampleWarningCode::EpochOutsideRange);
        sample.diagnosticText = "Earth-orientation sample used the last available row as a degraded fallback.";
    } else {
        sample = interpolateSamples(utcEpoch, *(lowerBound - 1), *lowerBound);
    }

    applyDataWarnings(sample, provider->dataInfo(), utcEpoch, options);
    if (sample.status == EarthOrientationSampleStatus::Degraded
        && sample.diagnosticText == "Earth-orientation sample resolved.") {
        sample.diagnosticText = "Earth-orientation sample resolved with degraded metadata.";
    }
    if (sample.status == EarthOrientationSampleStatus::Degraded
        && sample.diagnosticText == "Earth-orientation sample interpolated.") {
        sample.diagnosticText = "Earth-orientation sample interpolated with degraded metadata.";
    }
    return sample;
}

EarthOrientationSample sampleEarthOrientation(
    const std::shared_ptr<const IEarthOrientationProvider>& provider,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampleOptions& options
)
{
    return sampleEarthOrientation(provider.get(), utcEpoch, options);
}

}  // namespace skygate::ephemeris
