#include "DeltaTDataParser.hpp"
#include "EphemerisTableMetadata.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/CalendarTime.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kDataLabel = "Delta T data";
constexpr std::string_view kRowLabel = "Delta T";
constexpr std::string_view kValidityRangeId = "delta-t";
constexpr std::string_view kAncientFallbackRangeId = "delta-t-ancient-fallback";
constexpr std::string_view kAncientFallbackRangeDisplayName = "Ancient Delta T fallback model";

[[nodiscard]] std::optional<std::string> setFallbackStart(
    IDeltaTProvider::DataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackStart
)
{
    IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
        info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
    if (std::optional<std::string> diagnosticText = EphemerisTableMetadata::assignRangeEndpoint(
            fallback.validityRange,
            &EphemerisDateRange::start,
            value,
            kAncientFallbackRangeId,
            kAncientFallbackRangeDisplayName,
            kDataLabel,
            "ancient fallback start",
            lineNumber,
            hasFallbackStart
        );
        diagnosticText.has_value()) {
        return diagnosticText;
    }

    info.ancientFallbackModel = std::move(fallback);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> setFallbackEnd(
    IDeltaTProvider::DataInfo& info, const std::string& value, const std::size_t lineNumber, bool& hasFallbackEnd
)
{
    IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
        info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
    if (std::optional<std::string> diagnosticText = EphemerisTableMetadata::assignRangeEndpoint(
            fallback.validityRange,
            &EphemerisDateRange::end,
            value,
            kAncientFallbackRangeId,
            kAncientFallbackRangeDisplayName,
            kDataLabel,
            "ancient fallback end",
            lineNumber,
            hasFallbackEnd
        );
        diagnosticText.has_value()) {
        return diagnosticText;
    }

    info.ancientFallbackModel = std::move(fallback);
    return std::nullopt;
}

[[nodiscard]] bool applyMetadataLine(
    IDeltaTProvider::DataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasFallbackStart,
    bool& hasFallbackEnd,
    std::optional<std::string>& diagnosticText
)
{
    if (EphemerisTableMetadata::applyCommonLine(info, line, kDataLabel, lineNumber, diagnosticText)) {
        return true;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_start");
        value.has_value()) {
        diagnosticText = setFallbackStart(info, *value, lineNumber, hasFallbackStart);
        return true;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_end");
        value.has_value()) {
        diagnosticText = setFallbackEnd(info, *value, lineNumber, hasFallbackEnd);
        return true;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_source");
        value.has_value()) {
        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.provenance = std::move(*value);
        info.ancientFallbackModel = std::move(fallback);
        return true;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_delta_t_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value()) {
            diagnosticText =
                EphemerisTableMetadata::malformedMetadata(kDataLabel, "ancient fallback estimate", lineNumber);
            return true;
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.representativeDeltaTSeconds = *seconds;
        info.ancientFallbackModel = std::move(fallback);
        return true;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_uncertainty_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value() || *seconds < 0.0) {
            diagnosticText =
                EphemerisTableMetadata::malformedMetadata(kDataLabel, "ancient fallback uncertainty", lineNumber);
            return true;
        }

        IDeltaTProvider::DataInfo::FallbackModelInfo fallback =
            info.ancientFallbackModel.value_or(IDeltaTProvider::DataInfo::FallbackModelInfo{});
        fallback.estimatedUncertaintySeconds = *seconds;
        info.ancientFallbackModel = std::move(fallback);
        return true;
    }

    return true;
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
failureResult(IDeltaTProvider::DataInfo info, const EphemerisTableDataStatus status, std::string diagnosticText)
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
    std::optional<std::string> malformedLine = EphemerisTableMetadata::parseTableLines(
        asset.content,
        [&info, &hasFallbackStart, &hasFallbackEnd](
            const std::string_view line, const std::size_t lineNumber, std::optional<std::string>& diagnosticText
        ) { return applyMetadataLine(info, line, lineNumber, hasFallbackStart, hasFallbackEnd, diagnosticText); },
        [](const std::string_view, const std::string_view, const bool) noexcept { return false; },
        [](const std::string_view, const std::string_view trimmedLine, IDeltaTProvider::TableEntry& entry) noexcept {
            return parseEntryLine(trimmedLine, entry);
        },
        entries,
        kDataLabel
    );
    if (malformedLine.has_value()) {
        return failureResult(std::move(info), EphemerisTableDataStatus::Malformed, std::move(*malformedLine));
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

    if (std::optional<std::string> diagnosticText = EphemerisTableMetadata::finalizeParsedTable(
            info, entries, kDataLabel, kRowLabel, kValidityRangeId, true, false
        );
        diagnosticText.has_value()) {
        return failureResult(std::move(info), EphemerisTableDataStatus::Malformed, std::move(*diagnosticText));
    }

    DeltaTDataParser::Result result;
    result.dataInfo = std::move(info);
    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
