#include "DeltaTDataLoader.hpp"
#include "HighPrecisionTextParser.hpp"
#include "TableBackedDeltaTProvider.hpp"
#include "time/CalendarTime.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kValidityRangeId = "delta-t";
constexpr std::string_view kValidityRangeDisplayName = "Delta T data";
constexpr std::string_view kAncientFallbackRangeId = "delta-t-ancient-fallback";
constexpr std::string_view kAncientFallbackRangeDisplayName = "Ancient Delta T fallback model";

[[nodiscard]] const HighPrecisionTextParser& textParser() noexcept
{
    static const HighPrecisionTextParser parser;
    return parser;
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

[[nodiscard]] std::optional<std::string> setFallbackStart(
    IDeltaTProvider::DataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackStart
)
{
    const std::optional<CivilDateTime> date = textParser().parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch =
        date.has_value() ? CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Delta T data contains malformed ancient fallback start metadata at line " + std::to_string(lineNumber)
               + ".";
    }

    IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
        info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
    fallback.validityRange.id = kAncientFallbackRangeId;
    fallback.validityRange.displayName = kAncientFallbackRangeDisplayName;
    fallback.validityRange.start = *epoch;
    info.ancientFallbackModel = std::move(fallback);
    hasFallbackStart = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> setFallbackEnd(
    IDeltaTProvider::DataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackEnd
)
{
    const std::optional<CivilDateTime> date = textParser().parseUtcDate(value);
    const std::optional<AstronomicalEpoch> epoch =
        date.has_value() ? CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return "Delta T data contains malformed ancient fallback end metadata at line " + std::to_string(lineNumber)
               + ".";
    }

    IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
        info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
    fallback.validityRange.id = kAncientFallbackRangeId;
    fallback.validityRange.displayName = kAncientFallbackRangeDisplayName;
    fallback.validityRange.end = *epoch;
    info.ancientFallbackModel = std::move(fallback);
    hasFallbackEnd = true;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> applyMetadataLine(
    IDeltaTProvider::DataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasFallbackStart,
    bool& hasFallbackEnd
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
        const std::optional<CivilDateTime> expiresDate = textParser().parseUtcDate(*value);
        const std::optional<AstronomicalEpoch> expiresEpoch =
            expiresDate.has_value() ? CalendarTime::astronomicalEpochFromCivilDateTime(*expiresDate) : std::nullopt;
        if (!expiresEpoch.has_value()) {
            return "Delta T data contains malformed expiration metadata at line " + std::to_string(lineNumber) + ".";
        }

        info.expiresAt = *expiresEpoch;
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "ancient_fallback_start");
        value.has_value()) {
        return setFallbackStart(info, *value, lineNumber, hasFallbackStart);
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "ancient_fallback_end");
        value.has_value()) {
        return setFallbackEnd(info, *value, lineNumber, hasFallbackEnd);
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "ancient_fallback_source");
        value.has_value()) {
        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.provenance = std::move(*value);
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "ancient_fallback_delta_t_seconds");
        value.has_value()) {
        double seconds = 0.0;
        if (!textParser().parseFiniteDouble(*value, seconds)) {
            return "Delta T data contains malformed ancient fallback estimate metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.representativeDeltaTSeconds = seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value = textParser().metadataValue(line, "ancient_fallback_uncertainty_seconds");
        value.has_value()) {
        double seconds = 0.0;
        if (!textParser().parseFiniteDouble(*value, seconds) || seconds < 0.0) {
            return "Delta T data contains malformed ancient fallback uncertainty metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.estimatedUncertaintySeconds = seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }

    return std::nullopt;
}

[[nodiscard]] bool parseUsnoEntryLine(std::string_view line, IDeltaTProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = textParser().splitAsciiWhitespace(line);
    if (columns.size() < 2U) {
        return false;
    }

    CivilDateTime effectiveDate;
    double deltaTSeconds = 0.0;
    int year = 0;
    int month = 0;
    int day = 1;
    if (columns.size() >= 4U && textParser().parseInt(columns[0], year) && textParser().parseInt(columns[1], month)
        && textParser().parseInt(columns[2], day) && month >= 1 && month <= 12
        && textParser().parseFiniteDouble(columns[3], deltaTSeconds)) {
        effectiveDate.astronomicalYear = year;
        effectiveDate.month = month;
        effectiveDate.day = day;
        effectiveDate.timeScale = TimeScale::Utc;
    } else if (
        columns.size() >= 3U && textParser().parseInt(columns[0], year) && textParser().parseInt(columns[1], month)
        && month >= 1 && month <= 12 && textParser().parseFiniteDouble(columns[2], deltaTSeconds)
    ) {
        effectiveDate.astronomicalYear = year;
        effectiveDate.month = month;
        effectiveDate.day = 1;
        effectiveDate.timeScale = TimeScale::Utc;
    } else {
        double decimalYear = 0.0;
        if (!textParser().parseFiniteDouble(columns[0], decimalYear)
            || !textParser().parseFiniteDouble(columns[1], deltaTSeconds)) {
            return false;
        }
        effectiveDate = CalendarTime::civilDateFromDecimalYear(decimalYear);
    }

    if (!CalendarTime::isValidCivilDateTime(effectiveDate)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch =
        CalendarTime::astronomicalEpochFromCivilDateTime(effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcDate = effectiveDate;
    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.deltaTSeconds = deltaTSeconds;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, IDeltaTProvider::TableEntry& entry) noexcept
{
    const std::size_t comma = line.find(',');
    if (comma == std::string_view::npos) {
        return parseUsnoEntryLine(line, entry);
    }

    const std::optional<CivilDateTime> effectiveDate = textParser().parseUtcDate(line.substr(0U, comma));
    double deltaTSeconds = 0.0;
    if (!effectiveDate.has_value() || !textParser().parseFiniteDouble(line.substr(comma + 1U), deltaTSeconds)) {
        return false;
    }

    const std::optional<AstronomicalEpoch> effectiveEpoch =
        CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
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
    return textParser().startsWith(line, "effective_utc_date");
}

[[nodiscard]] DeltaTDataLoader::Result
failureResult(IDeltaTProvider::DataInfo info, const IDeltaTProvider::DataStatus status, std::string diagnosticText)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    DeltaTDataLoader::Result result;
    result.dataInfo = std::move(info);
    return result;
}

}  // namespace

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.deltaTDataAsset();
    if (!asset.has_value()) {
        IDeltaTProvider::DataInfo info;
        return failureResult(
            std::move(info),
            IDeltaTProvider::DataStatus::Missing,
            "Delta T data asset is missing from the data snapshot."
        );
    }

    return loadFromTextAsset(*asset, options);
}

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    IDeltaTProvider::DataInfo info;
    info.version = asset.version;
    info.provenance = asset.provenance;

    std::vector<IDeltaTProvider::TableEntry> entries;
    bool hasFallbackStart = false;
    bool hasFallbackEnd = false;
    std::string_view remaining = asset.content;
    std::size_t lineNumber = 0U;
    while (!remaining.empty()) {
        ++lineNumber;
        std::string_view line = textParser().takeLine(remaining);

        line = textParser().trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (textParser().startsWith(line, "#@ ")) {
            if (std::optional<std::string> diagnosticText =
                    applyMetadataLine(info, line, lineNumber, hasFallbackStart, hasFallbackEnd);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), IDeltaTProvider::DataStatus::Malformed, std::move(*diagnosticText)
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

        IDeltaTProvider::TableEntry entry;
        if (!parseEntryLine(line, entry)) {
            return failureResult(
                std::move(info),
                IDeltaTProvider::DataStatus::Malformed,
                "Delta T data contains a malformed row at line " + std::to_string(lineNumber) + "."
            );
        }
        entries.push_back(entry);
    }

    if (entries.empty()) {
        return failureResult(std::move(info), IDeltaTProvider::DataStatus::Malformed, "Delta T data contains no rows.");
    }

    const bool sorted = std::ranges::is_sorted(entries, {}, [](const IDeltaTProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });
    if (!sorted) {
        return failureResult(
            std::move(info), IDeltaTProvider::DataStatus::Malformed, "Delta T rows are not sorted by UTC date."
        );
    }

    if (info.ancientFallbackModel.has_value()) {
        IDeltaTProvider::DataInfo::FallbackModelInfo fallback = std::move(*info.ancientFallbackModel);
        if (!hasFallbackStart || !hasFallbackEnd || !fallback.validityRange.start.isFinite()
            || !fallback.validityRange.end.isFinite()
            || fallback.validityRange.start.sortKey() > fallback.validityRange.end.sortKey()) {
            return failureResult(
                std::move(info),
                IDeltaTProvider::DataStatus::Malformed,
                "Delta T ancient fallback metadata must include a valid start and end range."
            );
        }
        if (!fallback.representativeDeltaTSeconds.has_value()) {
            return failureResult(
                std::move(info),
                IDeltaTProvider::DataStatus::Malformed,
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
    info.status = IDeltaTProvider::DataStatus::Available;
    info.diagnosticText = "Delta T data loaded.";
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && options.referenceEpoch->isFinite()
        && options.referenceEpoch->sortKey() > info.expiresAt->sortKey()) {
        info.status = IDeltaTProvider::DataStatus::Stale;
        info.diagnosticText = "Delta T data is stale for the reference epoch.";
    }

    Result result;
    result.dataInfo = info;
    result.provider = std::make_shared<TableBackedDeltaTProvider>(std::move(info), std::move(entries));
    return result;
}

}  // namespace skygate::ephemeris
