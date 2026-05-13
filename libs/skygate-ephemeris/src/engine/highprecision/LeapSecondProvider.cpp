#include "skygate/ephemeris/LeapSecondProvider.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kValidityRangeId = "leap-seconds";
constexpr std::string_view kValidityRangeDisplayName = "Leap-second table";

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

[[nodiscard]] std::optional<CivilDateTime> parseUtcDate(std::string_view text) noexcept
{
    text = trimAsciiWhitespace(text);
    const std::size_t firstDash = text.find('-');
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

[[nodiscard]] std::string copyMetadataValue(const std::string_view line, const std::string_view key)
{
    const std::string prefix = "#@ " + std::string(key);
    if (!startsWith(line, prefix)) {
        return {};
    }

    return std::string(trimAsciiWhitespace(line.substr(prefix.size())));
}

void applyMetadataLine(LeapSecondTableInfo& info, const std::string_view line)
{
    if (std::string value = copyMetadataValue(line, "version"); !value.empty()) {
        info.version = std::move(value);
        return;
    }
    if (std::string value = copyMetadataValue(line, "source"); !value.empty()) {
        info.provenance = std::move(value);
        return;
    }
    if (std::string value = copyMetadataValue(line, "expires"); !value.empty()) {
        const std::optional<CivilDateTime> expiresDate = parseUtcDate(value);
        if (expiresDate.has_value()) {
            info.expiresAt = epochFromUtcDate(*expiresDate);
        }
    }
}

[[nodiscard]] bool parseEntryLine(std::string_view line, LeapSecondTableEntry& entry) noexcept
{
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }

    const std::optional<CivilDateTime> effectiveDate = parseUtcDate(line.substr(0U, comma));
    int taiMinusUtcSeconds = 0;
    if (!effectiveDate.has_value() || !parseInt(line.substr(comma + 1U), taiMinusUtcSeconds)) {
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

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return startsWith(line, "effective_utc_date");
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

        line = trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (startsWith(line, "#@ ")) {
            applyMetadataLine(info, line);
            continue;
        }
        if (startsWith(line, "#")) {
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
