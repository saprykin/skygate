#include "DeltaTDataParser.hpp"
#include "EphemerisTableMetadata.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"
#include "TextParsingUtilities.hpp"
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

[[nodiscard]] std::optional<std::string> setFallbackStart(
    IDeltaTProvider::DataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackStart
)
{
    const std::optional<skygate::core::CivilDateTime> date = TextParsingUtilities::parseUtcDate(value);
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
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
    const std::optional<skygate::core::CivilDateTime> date = TextParsingUtilities::parseUtcDate(value);
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
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
    std::optional<std::string> diagnosticText;
    if (EphemerisTableMetadata::applyCommonLine(info, line, "Delta T data", lineNumber, diagnosticText)) {
        return diagnosticText;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_start");
        value.has_value()) {
        return setFallbackStart(info, *value, lineNumber, hasFallbackStart);
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_end");
        value.has_value()) {
        return setFallbackEnd(info, *value, lineNumber, hasFallbackEnd);
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_source");
        value.has_value()) {
        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.provenance = std::move(*value);
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_delta_t_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value()) {
            return "Delta T data contains malformed ancient fallback estimate metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.representativeDeltaTSeconds = *seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_uncertainty_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value() || *seconds < 0.0) {
            return "Delta T data contains malformed ancient fallback uncertainty metadata at line "
                   + std::to_string(lineNumber) + ".";
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.estimatedUncertaintySeconds = *seconds;
        info.ancientFallbackModel = std::move(fallback);
        return std::nullopt;
    }

    return std::nullopt;
}

[[nodiscard]] bool parseUsnoEntryLine(std::string_view line, IDeltaTProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitAsciiWhitespace(line);
    if (columns.size() < 2U) {
        return false;
    }

    skygate::core::CivilDateTimeResult effectiveDate;
    double deltaTSeconds = 0.0;
    const std::optional<int> year = NumericParsingUtilities::parseInt(columns[0]);
    const std::optional<int> month = NumericParsingUtilities::parseInt(columns[1]);
    const std::optional<int> day = columns.size() >= 4U ? NumericParsingUtilities::parseInt(columns[2]) : std::nullopt;
    const std::optional<double> fourthColumnSeconds =
        columns.size() >= 4U ? NumericParsingUtilities::parseFiniteDouble(columns[3]) : std::nullopt;
    const std::optional<double> thirdColumnSeconds =
        columns.size() >= 3U ? NumericParsingUtilities::parseFiniteDouble(columns[2]) : std::nullopt;
    if (columns.size() >= 4U && year.has_value() && month.has_value() && day.has_value() && *month >= 1 && *month <= 12
        && fourthColumnSeconds.has_value()) {
        deltaTSeconds = *fourthColumnSeconds;
        effectiveDate = skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, *day);
    } else if (
        columns.size() >= 3U && year.has_value() && month.has_value() && *month >= 1 && *month <= 12
        && thirdColumnSeconds.has_value()
    ) {
        deltaTSeconds = *thirdColumnSeconds;
        effectiveDate = skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, 1);
    } else {
        const std::optional<double> decimalYear = NumericParsingUtilities::parseFiniteDouble(columns[0]);
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(columns[1]);
        if (!decimalYear.has_value() || !seconds.has_value()) {
            return false;
        }
        deltaTSeconds = *seconds;
        effectiveDate = skygate::core::CalendarTime::civilDateFromDecimalYear(*decimalYear);
    }

    if (!effectiveDate.has_value()) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.deltaTSeconds = deltaTSeconds;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, IDeltaTProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitCommaSeparated(line);
    if (columns.size() < 2U) {
        return parseUsnoEntryLine(line, entry);
    }
    if (columns.size() > 2U) {
        return false;
    }

    const std::optional<skygate::core::CivilDateTime> effectiveDate = TextParsingUtilities::parseUtcDate(columns[0]);
    const std::optional<double> deltaTSeconds = NumericParsingUtilities::parseFiniteDouble(columns[1]);
    if (!effectiveDate.has_value() || !deltaTSeconds.has_value()) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.deltaTSeconds = *deltaTSeconds;
    return true;
}

[[nodiscard]] DeltaTDataParser::Result
failureResult(IDeltaTProvider::DataInfo info, const IDeltaTProvider::DataStatus status, std::string diagnosticText)
{
    info.status = status;
    info.diagnosticText = std::move(diagnosticText);

    DeltaTDataParser::Result result;
    result.dataInfo = std::move(info);
    return result;
}

}  // namespace

DeltaTDataParser::Result DeltaTDataParser::parse(const EphemerisTextDataAsset& asset)
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
        std::string_view line = StringUtilities::takeLine(remaining);

        line = StringUtilities::trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (line.starts_with("#@ ")) {
            if (std::optional<std::string> diagnosticText =
                    applyMetadataLine(info, line, lineNumber, hasFallbackStart, hasFallbackEnd);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), IDeltaTProvider::DataStatus::Malformed, std::move(*diagnosticText)
                );
            }
            continue;
        }
        if (line.starts_with("#")) {
            continue;
        }
        if (EphemerisTableMetadata::isHeaderLine(line)) {
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

    const auto duplicate = std::ranges::adjacent_find(entries, {}, [](const IDeltaTProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });
    if (duplicate != entries.end()) {
        return failureResult(
            std::move(info), IDeltaTProvider::DataStatus::Malformed, "Delta T rows contain duplicate UTC dates."
        );
    }

    if (info.ancientFallbackModel.has_value()) {
        IDeltaTProvider::DataInfo::FallbackModelInfo fallback = std::move(*info.ancientFallbackModel);
        if (!hasFallbackStart || !hasFallbackEnd || !fallback.validityRange.isValid()) {
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

    info.validityRange = EphemerisTableMetadata::makeRange(
        kValidityRangeId, kValidityRangeDisplayName, entries.front().effectiveUtcEpoch, entries.back().effectiveUtcEpoch
    );
    info.status = IDeltaTProvider::DataStatus::Available;
    info.diagnosticText = "Delta T data loaded.";

    DeltaTDataParser::Result result;
    result.dataInfo = std::move(info);
    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
