#include "skygate/ephemeris/DeltaTProvider.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kValidityRangeId = "delta-t";
constexpr std::string_view kValidityRangeDisplayName = "Delta T data";
constexpr std::string_view kAncientFallbackRangeId = "delta-t-ancient-fallback";
constexpr std::string_view kAncientFallbackRangeDisplayName = "Ancient Delta T fallback model";

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

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
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

[[nodiscard]] std::optional<std::string>
setFallbackStart(DeltaTDataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackStart)
{
    const std::optional<CivilDateTime> date = parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch = date.has_value() ? epochFromUtcDate(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Delta T data contains malformed ancient fallback start metadata at line " + std::to_string(lineNumber)
               + ".";
    }

    DeltaTFallbackModelInfo fallback = info.ancientFallbackModel.value_or(DeltaTFallbackModelInfo{});
    fallback.validityRange.id = kAncientFallbackRangeId;
    fallback.validityRange.displayName = kAncientFallbackRangeDisplayName;
    fallback.validityRange.start = *epoch;
    info.ancientFallbackModel = std::move(fallback);
    hasFallbackStart = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string>
setFallbackEnd(DeltaTDataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackEnd)
{
    const std::optional<CivilDateTime> date = parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch = date.has_value() ? epochFromUtcDate(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Delta T data contains malformed ancient fallback end metadata at line " + std::to_string(lineNumber)
               + ".";
    }

    DeltaTFallbackModelInfo fallback = info.ancientFallbackModel.value_or(DeltaTFallbackModelInfo{});
    fallback.validityRange.id = kAncientFallbackRangeId;
    fallback.validityRange.displayName = kAncientFallbackRangeDisplayName;
    fallback.validityRange.end = *epoch;
    info.ancientFallbackModel = std::move(fallback);
    hasFallbackEnd = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> applyMetadataLine(
    DeltaTDataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasFallbackStart,
    bool& hasFallbackEnd
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
            return "Delta T data contains malformed expiration metadata at line " + std::to_string(lineNumber) + ".";
        }

        info.expiresAt = *expiresEpoch;
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "ancient_fallback_start"); value.has_value()) {
        return setFallbackStart(info, *value, lineNumber, hasFallbackStart);
    }
    if (std::optional<std::string> value = metadataValue(line, "ancient_fallback_end"); value.has_value()) {
        return setFallbackEnd(info, *value, lineNumber, hasFallbackEnd);
    }
    if (std::optional<std::string> value = metadataValue(line, "ancient_fallback_source"); value.has_value()) {
        DeltaTFallbackModelInfo fallback = info.ancientFallbackModel.value_or(DeltaTFallbackModelInfo{});
        fallback.provenance = std::move(*value);
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "ancient_fallback_delta_t_seconds"); value.has_value()) {
        double seconds = 0.0;
        if (!parseDouble(*value, seconds)) {
            return "Delta T data contains malformed ancient fallback estimate metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        DeltaTFallbackModelInfo fallback = info.ancientFallbackModel.value_or(DeltaTFallbackModelInfo{});
        fallback.representativeDeltaTSeconds = seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value = metadataValue(line, "ancient_fallback_uncertainty_seconds");
        value.has_value()) {
        double seconds = 0.0;
        if (!parseDouble(*value, seconds) || seconds < 0.0) {
            return "Delta T data contains malformed ancient fallback uncertainty metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        DeltaTFallbackModelInfo fallback = info.ancientFallbackModel.value_or(DeltaTFallbackModelInfo{});
        fallback.estimatedUncertaintySeconds = seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }

    return std::nullopt;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, DeltaTTableEntry& entry) noexcept
{
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        return false;
    }

    const std::optional<CivilDateTime> effectiveDate = parseUtcDate(line.substr(0U, comma));
    double deltaTSeconds = 0.0;
    if (!effectiveDate.has_value() || !parseDouble(line.substr(comma + 1U), deltaTSeconds)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch = epochFromUtcDate(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = *effectiveDate;
    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.deltaTSeconds = deltaTSeconds;
    return true;
}

[[nodiscard]] bool isHeaderLine(const std::string_view line) noexcept
{
    return startsWith(line, "effective_utc_date");
}

[[nodiscard]] DeltaTDataLoadResult
failureResult(DeltaTDataInfo info, const DeltaTDataStatus status, std::string diagnosticText)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    DeltaTDataLoadResult result;
    result.dataInfo = std::move(info);
    return result;
}

[[nodiscard]] bool epochWithinRange(const AstronomicalEpoch& epoch, const EphemerisDateRange& range) noexcept
{
    if (!isFiniteEpoch(epoch)) {
        return false;
    }

    const double key = epochSortKey(epoch);
    return key >= epochSortKey(range.start) && key <= epochSortKey(range.end);
}

[[nodiscard]] DeltaTEstimate unavailableEstimate(std::string diagnosticText)
{
    DeltaTEstimate estimate;
    estimate.status = DeltaTEstimateStatus::Unavailable;
    estimate.diagnosticText = std::move(diagnosticText);
    return estimate;
}

}  // namespace

TableBackedDeltaTProvider::TableBackedDeltaTProvider(DeltaTDataInfo dataInfo, std::vector<DeltaTTableEntry> entries)
    : m_dataInfo(std::move(dataInfo)), m_entries(std::move(entries))
{
}

const DeltaTDataInfo& TableBackedDeltaTProvider::dataInfo() const noexcept
{
    return m_dataInfo;
}

std::span<const DeltaTTableEntry> TableBackedDeltaTProvider::entries() const noexcept
{
    return m_entries;
}

DeltaTEstimate TableBackedDeltaTProvider::deltaTSeconds(const AstronomicalEpoch& epoch) const
{
    if (!isFiniteEpoch(epoch)) {
        return unavailableEstimate("Delta T estimate requires a finite epoch.");
    }

    if (m_entries.empty()) {
        return unavailableEstimate("Delta T data contains no table entries.");
    }

    const double requestedEpochKey = epochSortKey(epoch);
    if (requestedEpochKey >= epochSortKey(m_entries.front().effectiveUtcEpoch)
        && requestedEpochKey <= epochSortKey(m_entries.back().effectiveUtcEpoch)) {
        const auto upper =
            std::ranges::lower_bound(m_entries, requestedEpochKey, {}, [](const DeltaTTableEntry& entry) {
                return epochSortKey(entry.effectiveUtcEpoch);
            });
        if (upper == m_entries.begin()) {
            return DeltaTEstimate{
                .status = DeltaTEstimateStatus::Available,
                .deltaTSeconds = upper->deltaTSeconds,
                .diagnosticText = "Delta T estimate loaded from table.",
                .provenance = m_dataInfo.provenance,
            };
        }
        if (upper == m_entries.end()) {
            return DeltaTEstimate{
                .status = DeltaTEstimateStatus::Available,
                .deltaTSeconds = m_entries.back().deltaTSeconds,
                .diagnosticText = "Delta T estimate loaded from table.",
                .provenance = m_dataInfo.provenance,
            };
        }

        const DeltaTTableEntry& before = *(upper - 1);
        const DeltaTTableEntry& after = *upper;
        const double beforeKey = epochSortKey(before.effectiveUtcEpoch);
        const double afterKey = epochSortKey(after.effectiveUtcEpoch);
        const double ratio = (requestedEpochKey - beforeKey) / (afterKey - beforeKey);
        return DeltaTEstimate{
            .status = DeltaTEstimateStatus::Available,
            .deltaTSeconds = before.deltaTSeconds + (after.deltaTSeconds - before.deltaTSeconds) * ratio,
            .diagnosticText = "Delta T estimate interpolated from table.",
            .provenance = m_dataInfo.provenance,
        };
    }

    if (m_dataInfo.ancientFallbackModel.has_value()
        && epochWithinRange(epoch, m_dataInfo.ancientFallbackModel->validityRange)) {
        const DeltaTFallbackModelInfo& fallback = *m_dataInfo.ancientFallbackModel;
        return DeltaTEstimate{
            .status = DeltaTEstimateStatus::Degraded,
            .deltaTSeconds = fallback.representativeDeltaTSeconds,
            .estimatedUncertaintySeconds = fallback.estimatedUncertaintySeconds,
            .diagnosticText = "Delta T estimate uses ancient fallback model metadata.",
            .provenance = fallback.provenance,
        };
    }

    return unavailableEstimate("Requested epoch is outside Delta T data coverage.");
}

DeltaTDataLoadResult
loadDeltaTDataFromSnapshot(const IEphemerisDataSnapshot& snapshot, const DeltaTDataLoadOptions& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.deltaTDataAsset();
    if (!asset.has_value()) {
        DeltaTDataInfo info;
        return failureResult(
            std::move(info), DeltaTDataStatus::Missing, "Delta T data asset is missing from the data snapshot."
        );
    }

    return loadDeltaTDataFromTextAsset(*asset, options);
}

DeltaTDataLoadResult
loadDeltaTDataFromTextAsset(const EphemerisTextDataAsset& asset, const DeltaTDataLoadOptions& options)
{
    DeltaTDataInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<DeltaTTableEntry> entries;
    bool hasFallbackStart = false;
    bool hasFallbackEnd = false;
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
            if (std::optional<std::string> diagnosticText =
                    applyMetadataLine(info, line, lineNumber, hasFallbackStart, hasFallbackEnd);
                diagnosticText.has_value()) {
                return failureResult(std::move(info), DeltaTDataStatus::Malformed, std::move(*diagnosticText));
            }
            continue;
        }
        if (startsWith(line, "#")) {
            continue;
        }
        if (isHeaderLine(line)) {
            continue;
        }

        DeltaTTableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                DeltaTDataStatus::Malformed,
                "Delta T data contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(std::move(info), DeltaTDataStatus::Malformed, "Delta T data contains no rows.");
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const DeltaTTableEntry& entry) {
        return epochSortKey(entry.effectiveUtcEpoch);
    });
    if (!sorted) {
        return failureResult(std::move(info), DeltaTDataStatus::Malformed, "Delta T rows are not sorted by UTC date.");
    }

    if (info.ancientFallbackModel.has_value()) {
        DeltaTFallbackModelInfo fallback = std::move(*info.ancientFallbackModel);
        if (!hasFallbackStart || !hasFallbackEnd || !isFiniteEpoch(fallback.validityRange.start)
            || !isFiniteEpoch(fallback.validityRange.end)
            || epochSortKey(fallback.validityRange.start) > epochSortKey(fallback.validityRange.end)) {
            return failureResult(
                std::move(info),
                DeltaTDataStatus::Malformed,
                "Delta T ancient fallback metadata must include a valid start and end range."
            );
        }
        if (!fallback.representativeDeltaTSeconds.has_value()) {
            return failureResult(
                std::move(info),
                DeltaTDataStatus::Malformed,
                "Delta T ancient fallback metadata must include a representative estimate."
            );
        }
        if (fallback.id.empty()) {
            fallback.id = "ancient-fallback";
        }
        if (fallback.displayName.empty()) {
            fallback.displayName = "Ancient fallback model";
        }
        info.ancientFallbackModel = std::move(fallback);
    }

    info.validityRange = makeRange(
        kValidityRangeId, kValidityRangeDisplayName, entries.front().effectiveUtcEpoch, entries.back().effectiveUtcEpoch
    );
    info.status = DeltaTDataStatus::Available;
    info.diagnosticText = "Delta T data loaded.";
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && isFiniteEpoch(*options.referenceEpoch)
        && epochSortKey(*options.referenceEpoch) > epochSortKey(*info.expiresAt)) {
        info.status = DeltaTDataStatus::Stale;
        info.diagnosticText = "Delta T data is stale for the reference epoch.";
    }

    DeltaTDataLoadResult result;
    result.dataInfo = info;
    result.provider = std::make_shared<TableBackedDeltaTProvider>(std::move(info), std::move(entries));
    return result;
}

}  // namespace skygate::ephemeris
