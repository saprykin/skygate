#include "DeltaTDataParser.hpp"
#include "EphemerisEpochParsing.hpp"
#include "EphemerisTableMetadata.hpp"
#include "EphemerisTableParsing.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/CalendarTime.hpp"
#include "time/GregorianCalendar.hpp"

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

constexpr EphemerisTableParsing::TableParseSpec kParseSpec{
    .dataLabel = kDataLabel,
    .rowLabel = kRowLabel,
    .validityRangeId = kValidityRangeId,
    .rejectDuplicateDates = true,
    .endValidityRangeAtExpiration = false,
};

[[nodiscard]] std::optional<std::string> setFallbackEndpoint(
    IDeltaTProvider::DataInfo& info,
    skygate::core::AstronomicalEpoch EphemerisDateRange::* const endpoint,
    const std::string_view subject,
    const std::string& value,
    const std::size_t lineNumber,
    bool& hasEndpoint
)
{
    return EphemerisTableMetadata::assignOptionalRangeEndpoint(
        info.ancientFallbackModel,
        [](IDeltaTProvider::DataInfo::FallbackModelInfo& fallback) noexcept -> EphemerisDateRange& {
            return fallback.validityRange;
        },
        endpoint,
        value,
        kAncientFallbackRangeId,
        kAncientFallbackRangeDisplayName,
        kDataLabel,
        subject,
        lineNumber,
        hasEndpoint
    );
}

void applyMetadataLine(
    IDeltaTProvider::DataInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    bool& hasFallbackStart,
    bool& hasFallbackEnd,
    std::optional<std::string>& diagnosticText
)
{
    if (EphemerisTableMetadata::applyCommonLine(info, line, kDataLabel, lineNumber, diagnosticText)) {
        return;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_start");
        value.has_value()) {
        diagnosticText = setFallbackEndpoint(
            info, &EphemerisDateRange::start, "ancient fallback start", *value, lineNumber, hasFallbackStart
        );
        return;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_end");
        value.has_value()) {
        diagnosticText = setFallbackEndpoint(
            info, &EphemerisDateRange::end, "ancient fallback end", *value, lineNumber, hasFallbackEnd
        );
        return;
    }
    if (std::optional<std::string> value = TextParsingUtilities::metadataValue(line, "ancient_fallback_source");
        value.has_value()) {
        EphemerisTableParsing::updateOptionalField(
            info.ancientFallbackModel, [&value](IDeltaTProvider::DataInfo::FallbackModelInfo& fallback) {
                fallback.provenance = std::move(*value);
            }
        );
        return;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_delta_t_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value()) {
            diagnosticText =
                EphemerisTableMetadata::malformedMetadata(kDataLabel, "ancient fallback estimate", lineNumber);
            return;
        }

        EphemerisTableParsing::updateOptionalField(
            info.ancientFallbackModel, [&seconds](IDeltaTProvider::DataInfo::FallbackModelInfo& fallback) {
                fallback.representativeDeltaTSeconds = *seconds;
            }
        );
        return;
    }
    if (std::optional<std::string> value =
            TextParsingUtilities::metadataValue(line, "ancient_fallback_uncertainty_seconds");
        value.has_value()) {
        const std::optional<double> seconds = NumericParsingUtilities::parseFiniteDouble(*value);
        if (!seconds.has_value() || *seconds < 0.0) {
            diagnosticText =
                EphemerisTableMetadata::malformedMetadata(kDataLabel, "ancient fallback uncertainty", lineNumber);
            return;
        }

        EphemerisTableParsing::updateOptionalField(
            info.ancientFallbackModel, [&seconds](IDeltaTProvider::DataInfo::FallbackModelInfo& fallback) {
                fallback.estimatedUncertaintySeconds = *seconds;
            }
        );
        return;
    }
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
    if (columns.size() >= 4U && year.has_value() && month.has_value() && day.has_value()
        && skygate::core::CalendarTime::isValidGregorianMonth(*month) && fourthColumnSeconds.has_value()) {
        deltaTSeconds = *fourthColumnSeconds;
        effectiveDate = skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, *day);
    } else if (
        columns.size() >= 3U && year.has_value() && month.has_value()
        && skygate::core::CalendarTime::isValidGregorianMonth(*month) && thirdColumnSeconds.has_value()
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
        effectiveDate = skygate::core::GregorianCalendar::civilDateFromDecimalYear(*decimalYear);
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromCivilDate(effectiveDate);
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

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromDateColumn(columns[0]);
    const std::optional<double> deltaTSeconds = NumericParsingUtilities::parseFiniteDouble(columns[1]);
    if (!effectiveEpoch.has_value() || !deltaTSeconds.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.deltaTSeconds = *deltaTSeconds;
    return true;
}

// The ancient fallback model is usable only when its metadata names a valid
// validity range and a representative estimate.
[[nodiscard]] std::optional<std::string>
checkAncientFallback(IDeltaTProvider::DataInfo& info, const bool hasFallbackStart, const bool hasFallbackEnd)
{
    if (!info.ancientFallbackModel.has_value()) {
        return std::nullopt;
    }

    IDeltaTProvider::DataInfo::FallbackModelInfo fallback = std::move(*info.ancientFallbackModel);
    if (!hasFallbackStart || !hasFallbackEnd || !fallback.validityRange.isValid()) {
        return "Delta T ancient fallback metadata must include a valid start and end range.";
    }
    if (!fallback.representativeDeltaTSeconds.has_value()) {
        return "Delta T ancient fallback metadata must include a representative estimate.";
    }
    if (fallback.id.empty()) {
        fallback.id = "ancient-fallback";
    }
    if (fallback.displayName.empty()) {
        fallback.displayName = "Ancient fallback model";
    }
    info.ancientFallbackModel = std::move(fallback);
    return std::nullopt;
}

}  // namespace

DeltaTDataParser::Result DeltaTDataParser::parse(const EphemerisTextDataAsset& asset)
{
    Result result;
    std::vector<IDeltaTProvider::TableEntry> entries;
    bool hasFallbackStart = false;
    bool hasFallbackEnd = false;
    if (std::optional<std::string> diagnosticText = EphemerisTableParsing::parsePayload(
            asset,
            result.dataInfo,
            entries,
            [&result, &hasFallbackStart, &hasFallbackEnd](
                const std::string_view line,
                const std::size_t lineNumber,
                std::optional<std::string>& metadataDiagnostic
            ) {
                applyMetadataLine(
                    result.dataInfo, line, lineNumber, hasFallbackStart, hasFallbackEnd, metadataDiagnostic
                );
            },
            EphemerisTableParsing::skipNoLines,
            [](const std::string_view,
               const std::string_view trimmedLine,
               IDeltaTProvider::TableEntry& entry) noexcept { return parseEntryLine(trimmedLine, entry); },
            kParseSpec,
            [&hasFallbackStart, &hasFallbackEnd](IDeltaTProvider::DataInfo& info) {
                return checkAncientFallback(info, hasFallbackStart, hasFallbackEnd);
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
