#include "LeapSecondTableParser.hpp"
#include "HighPrecisionTextParser.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "math/TimeConstants.hpp"
#include "time/CalendarTime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kValidityRangeId = "leap-seconds";
constexpr std::string_view kValidityRangeDisplayName = "Leap-second table";

[[nodiscard]] const HighPrecisionTextParser& textParser() noexcept
{
    static const HighPrecisionTextParser parser;
    return parser;
}

[[nodiscard]] std::optional<int> monthFromEnglishAbbreviation(const std::string_view text) noexcept
{
    if (text == "Jan") {
        return 1;
    }
    if (text == "Feb") {
        return 2;
    }
    if (text == "Mar") {
        return 3;
    }
    if (text == "Apr") {
        return 4;
    }
    if (text == "May") {
        return 5;
    }
    if (text == "Jun") {
        return 6;
    }
    if (text == "Jul") {
        return 7;
    }
    if (text == "Aug") {
        return 8;
    }
    if (text == "Sep") {
        return 9;
    }
    if (text == "Oct") {
        return 10;
    }
    if (text == "Nov") {
        return 11;
    }
    if (text == "Dec") {
        return 12;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<skygate::core::CivilDateTime> parseIanaCommentDate(const std::string_view text) noexcept
{
    const std::vector<std::string_view> tokens = textParser().splitAsciiWhitespace(text);
    if (tokens.size() < 3U) {
        return std::nullopt;
    }

    int day = 0;
    int year = 0;
    const std::optional<int> month = monthFromEnglishAbbreviation(tokens[1]);
    if (!textParser().parseInt(tokens[0], day) || !month.has_value() || !textParser().parseInt(tokens[2], year)) {
        return std::nullopt;
    }

    skygate::core::CivilDateTime dateTime;
    dateTime.astronomicalYear = year;
    dateTime.month = *month;
    dateTime.day = day;
    dateTime.timeScale = skygate::core::TimeScale::Utc;
    if (!skygate::core::CalendarTime::isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }

    return dateTime;
}

[[nodiscard]] skygate::core::AstronomicalEpoch epochFromNtpTimestamp(const std::uint64_t ntpTimestamp) noexcept
{
    constexpr double kModifiedJulianDateEpoch = 2'400'000.5;
    constexpr double kModifiedJulianDateAtNtpEpoch = 15'020.0;
    return skygate::core::AstronomicalEpoch{
        .julianDatePart1 = kModifiedJulianDateEpoch + kModifiedJulianDateAtNtpEpoch,
        .julianDatePart2 = static_cast<double>(ntpTimestamp) / skygate::core::TimeConstants::kSecondsPerDay,
        .timeScale = skygate::core::TimeScale::Utc,
    }
        .normalized();
}

[[nodiscard]] std::optional<std::string>
applyMetadataLine(ILeapSecondProvider::TableInfo& info, const std::string_view line, const std::size_t lineNumber)
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
        if (!expiresDate.has_value()) {
            return "Leap-second table contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        const std::optional<skygate::core::AstronomicalEpoch> expiresEpoch =
            skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*expiresDate);
        if (!expiresEpoch.has_value()) {
            return "Leap-second table contains unusable expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        info.expiresAt = *expiresEpoch;
        return std::nullopt;
    }
    if (textParser().startsWith(line, "#@")) {
        std::uint64_t ntpExpiration = 0U;
        if (!textParser().parseUint64(line.substr(2U), ntpExpiration)) {
            return "Leap-second table contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        info.expiresAt = epochFromNtpTimestamp(ntpExpiration);
    }

    return std::nullopt;
}

[[nodiscard]] bool parseCsvEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }

    const std::optional<skygate::core::CivilDateTime> effectiveDate = textParser().parseUtcDate(line.substr(0U, comma));
    int taiMinusUtcSeconds = 0;
    if (!effectiveDate.has_value() || !textParser().parseInt(line.substr(comma + 1U), taiMinusUtcSeconds)) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.taiMinusUtcSeconds = taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseIanaEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    const std::size_t commentOffset = line.find('#');
    const std::string_view payload = textParser().trimAsciiWhitespace(
        commentOffset == std::string_view::npos ? line : line.substr(0U, commentOffset)
    );
    const std::vector<std::string_view> columns = textParser().splitAsciiWhitespace(payload);
    if (columns.size() < 2U) {
        return false;
    }

    std::uint64_t ntpTimestamp = 0U;
    int taiMinusUtcSeconds = 0;
    if (!textParser().parseUint64(columns[0], ntpTimestamp) || !textParser().parseInt(columns[1], taiMinusUtcSeconds)) {
        return false;
    }

    const skygate::core::AstronomicalEpoch epoch = epochFromNtpTimestamp(ntpTimestamp);
    std::optional<skygate::core::CivilDateTime> effectiveDate;
    if (commentOffset != std::string_view::npos) {
        effectiveDate = parseIanaCommentDate(line.substr(commentOffset + 1U));
    }
    if (!effectiveDate.has_value()) {
        effectiveDate = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);
    }
    if (!effectiveDate.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = epoch;
    entry.taiMinusUtcSeconds = taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    return parseCsvEntryLine(line, entry) || parseIanaEntryLine(line, entry);
}

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return textParser().startsWith(line, "effective_utc_date");
}

[[nodiscard]] EphemerisDateRange makeValidityRange(
    const ILeapSecondProvider::TableEntry& firstEntry,
    const ILeapSecondProvider::TableEntry& lastEntry,
    const std::optional<skygate::core::AstronomicalEpoch>& expiresAt
)
{
    EphemerisDateRange range;
    range.id = kValidityRangeId;
    range.displayName = kValidityRangeDisplayName;
    range.start = firstEntry.effectiveUtcEpoch;
    range.end = expiresAt.value_or(lastEntry.effectiveUtcEpoch);
    return range;
}

[[nodiscard]] LeapSecondTableParser::Result failureResult(
    ILeapSecondProvider::TableInfo info, const ILeapSecondProvider::TableStatus status, std::string diagnosticText
)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    LeapSecondTableParser::Result result;
    result.tableInfo = std::move(info);
    return result;
}

}  // namespace

LeapSecondTableParser::Result LeapSecondTableParser::parse(const EphemerisTextDataAsset& asset)
{
    ILeapSecondProvider::TableInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<ILeapSecondProvider::TableEntry> entries;
    std::string_view remaining = asset.content;
    std::size_t lineNumber = 0U;
    while (!remaining.empty()) {
        ++lineNumber;
        std::string_view line = textParser().takeLine(remaining);

        line = textParser().trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (textParser().startsWith(line, "#@")) {
            if (std::optional<std::string> diagnosticText = applyMetadataLine(info, line, lineNumber);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), ILeapSecondProvider::TableStatus::Malformed, std::move(*diagnosticText)
                );
            }
            continue;
        }
        if (textParser().startsWith(line, "#")) {
            continue;
        }
        if (isHeaderLine(line)) {
            continue;
        }

        ILeapSecondProvider::TableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                ILeapSecondProvider::TableStatus::Malformed,
                "Leap-second table contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(
            std::move(info), ILeapSecondProvider::TableStatus::Malformed, "Leap-second table contains no rows."
        );
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const ILeapSecondProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });
    if (!sorted) {
        return failureResult(
            std::move(info),
            ILeapSecondProvider::TableStatus::Malformed,
            "Leap-second table rows are not sorted by UTC date."
        );
    }

    info.validityRange = makeValidityRange(entries.front(), entries.back(), info.expiresAt);
    info.status = ILeapSecondProvider::TableStatus::Available;
    info.diagnosticText = "Leap-second table loaded.";

    LeapSecondTableParser::Result result;
    result.tableInfo = std::move(info);
    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
