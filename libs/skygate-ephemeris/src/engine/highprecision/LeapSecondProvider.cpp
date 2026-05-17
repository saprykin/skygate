#include "skygate/ephemeris/LeapSecondProvider.hpp"

#include "engine/highprecision/HighPrecisionTextParser.hpp"

#include <algorithm>
#include <cmath>
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

[[nodiscard]] std::optional<CivilDateTime> parseIanaCommentDate(const std::string_view text) noexcept
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

    CivilDateTime dateTime;
    dateTime.astronomicalYear = year;
    dateTime.month = *month;
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

[[nodiscard]] AstronomicalEpoch epochFromNtpTimestamp(const std::uint64_t ntpTimestamp) noexcept
{
    constexpr double kModifiedJulianDateEpoch = 2'400'000.5;
    constexpr double kModifiedJulianDateAtNtpEpoch = 15'020.0;
    return normalizedAstronomicalEpoch(
        AstronomicalEpoch{
            .julianDatePart1 = kModifiedJulianDateEpoch + kModifiedJulianDateAtNtpEpoch,
            .julianDatePart2 = static_cast<double>(ntpTimestamp) / static_cast<double>(detail::kSecondsPerDay),
            .timeScale = TimeScale::Utc,
        }
    );
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

[[nodiscard]] std::optional<std::string>
applyMetadataLine(LeapSecondTableInfo& info, const std::string_view line, const std::size_t lineNumber)
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
        const std::optional<CivilDateTime> expiresDate = textParser().parseUtcDate(*value);
        if (!expiresDate.has_value()) {
            return "Leap-second table contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        const std::optional<AstronomicalEpoch> expiresEpoch = epochFromUtcDate(*expiresDate);
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

[[nodiscard]] bool parseCsvEntryLine(std::string_view line, LeapSecondTableEntry& entry) noexcept
{
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }

    const std::optional<CivilDateTime> effectiveDate = textParser().parseUtcDate(line.substr(0U, comma));
    int taiMinusUtcSeconds = 0;
    if (!effectiveDate.has_value() || !textParser().parseInt(line.substr(comma + 1U), taiMinusUtcSeconds)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch = epochFromUtcDate(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = *effectiveDate;
    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.taiMinusUtcSeconds = taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseIanaEntryLine(std::string_view line, LeapSecondTableEntry& entry) noexcept
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

    const AstronomicalEpoch epoch = epochFromNtpTimestamp(ntpTimestamp);
    std::optional<CivilDateTime> effectiveDate;
    if (commentOffset != std::string_view::npos) {
        effectiveDate = parseIanaCommentDate(line.substr(commentOffset + 1U));
    }
    if (!effectiveDate.has_value()) {
        effectiveDate = civilDateTimeFromAstronomicalEpoch(epoch);
    }
    if (!effectiveDate.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = *effectiveDate;
    entry.effectiveUtcEpoch = epoch;
    entry.taiMinusUtcSeconds = taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, LeapSecondTableEntry& entry) noexcept
{
    return parseCsvEntryLine(line, entry) || parseIanaEntryLine(line, entry);
}

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return textParser().startsWith(line, "effective_utc_date");
}

[[nodiscard]] EphemerisDateRange makeValidityRange(
    const LeapSecondTableEntry& firstEntry,
    const LeapSecondTableEntry& lastEntry,
    const std::optional<AstronomicalEpoch>& expiresAt
)
{
    EphemerisDateRange range;
    range.id = kValidityRangeId;
    range.displayName = kValidityRangeDisplayName;
    range.start = firstEntry.effectiveUtcEpoch;
    range.end = expiresAt.value_or(lastEntry.effectiveUtcEpoch);
    return range;
}

[[nodiscard]] LeapSecondTableLoadResult
failureResult(LeapSecondTableInfo info, const LeapSecondTableStatus status, std::string diagnosticText)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    LeapSecondTableLoadResult result;
    result.tableInfo = std::move(info);
    return result;
}

}  // namespace

TableBackedLeapSecondProvider::TableBackedLeapSecondProvider(
    LeapSecondTableInfo tableInfo, std::vector<LeapSecondTableEntry> entries
)
    : m_tableInfo(std::move(tableInfo)), m_entries(std::move(entries))
{
}

const LeapSecondTableInfo& TableBackedLeapSecondProvider::tableInfo() const noexcept
{
    return m_tableInfo;
}

std::span<const LeapSecondTableEntry> TableBackedLeapSecondProvider::entries() const noexcept
{
    return m_entries;
}

std::optional<int> TableBackedLeapSecondProvider::taiMinusUtcSeconds(const AstronomicalEpoch& utcEpoch) const noexcept
{
    if (!isFiniteUtcEpoch(utcEpoch)) {
        return std::nullopt;
    }

    const double requestedEpochKey = epochSortKey(utcEpoch);
    std::optional<int> offset;
    for (const LeapSecondTableEntry& entry : m_entries) {
        if (epochSortKey(entry.effectiveUtcEpoch) > requestedEpochKey) {
            break;
        }
        offset = entry.taiMinusUtcSeconds;
    }

    return offset;
}

LeapSecondTableLoadResult
loadLeapSecondTableFromSnapshot(const IEphemerisDataSnapshot& snapshot, const LeapSecondTableLoadOptions& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.leapSecondTableAsset();
    if (!asset.has_value()) {
        LeapSecondTableInfo info;
        return failureResult(
            std::move(info),
            LeapSecondTableStatus::Missing,
            "Leap-second table asset is missing from the data snapshot."
        );
    }

    return loadLeapSecondTableFromTextAsset(*asset, options);
}

LeapSecondTableLoadResult
loadLeapSecondTableFromTextAsset(const EphemerisTextDataAsset& asset, const LeapSecondTableLoadOptions& options)
{
    LeapSecondTableInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<LeapSecondTableEntry> entries;
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

        line = textParser().trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (textParser().startsWith(line, "#@")) {
            if (std::optional<std::string> diagnosticText = applyMetadataLine(info, line, lineNumber);
                diagnosticText.has_value()) {
                return failureResult(std::move(info), LeapSecondTableStatus::Malformed, std::move(*diagnosticText));
            }
            continue;
        }
        if (textParser().startsWith(line, "#")) {
            continue;
        }
        if (isHeaderLine(line)) {
            continue;
        }

        LeapSecondTableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                LeapSecondTableStatus::Malformed,
                "Leap-second table contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(std::move(info), LeapSecondTableStatus::Malformed, "Leap-second table contains no rows.");
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const LeapSecondTableEntry& entry) {
        return epochSortKey(entry.effectiveUtcEpoch);
    });
    if (!sorted) {
        return failureResult(
            std::move(info), LeapSecondTableStatus::Malformed, "Leap-second table rows are not sorted by UTC date."
        );
    }

    info.validityRange = makeValidityRange(entries.front(), entries.back(), info.expiresAt);
    info.status = LeapSecondTableStatus::Available;
    info.diagnosticText = "Leap-second table loaded.";
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && isFiniteUtcEpoch(*options.referenceEpoch)
        && epochSortKey(*options.referenceEpoch) > epochSortKey(*info.expiresAt)) {
        info.status = LeapSecondTableStatus::Stale;
        info.diagnosticText = "Leap-second table is stale for the reference epoch.";
    }

    LeapSecondTableLoadResult result;
    result.tableInfo = info;
    result.provider = std::make_shared<TableBackedLeapSecondProvider>(std::move(info), std::move(entries));
    return result;
}

}  // namespace skygate::ephemeris
