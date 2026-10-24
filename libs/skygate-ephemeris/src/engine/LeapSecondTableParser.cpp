#include "LeapSecondTableParser.hpp"
#include "EphemerisEpochParsing.hpp"
#include "EphemerisTableMetadata.hpp"
#include "EphemerisTableParsing.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/CalendarTime.hpp"
#include "time/EpochCodec.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kDataLabel = "Leap-second table";
constexpr std::string_view kValidityRangeId = "leap-seconds";
constexpr std::size_t kMetadataMarkerSize = EphemerisTableMetadata::kMetadataLinePrefix.size() - 1U;

constexpr EphemerisTableParsing::TableParseSpec kParseSpec{
    .dataLabel = kDataLabel,
    .rowLabel = kDataLabel,
    .validityRangeId = kValidityRangeId,
    .rejectDuplicateDates = false,
    .endValidityRangeAtExpiration = true,
};

[[nodiscard]] std::optional<skygate::core::CivilDateTime> parseIanaCommentDate(const std::string_view text) noexcept
{
    const std::vector<std::string_view> tokens = TextParsingUtilities::splitAsciiWhitespace(text);
    if (tokens.size() < 3U) {
        return std::nullopt;
    }

    const std::optional<int> day = NumericParsingUtilities::parseInt(tokens[0]);
    const std::optional<int> month = TextParsingUtilities::parseEnglishMonthAbbreviation(tokens[1]);
    const std::optional<int> year = NumericParsingUtilities::parseInt(tokens[2]);
    if (!day.has_value() || !month.has_value() || !year.has_value()) {
        return std::nullopt;
    }

    return skygate::core::CalendarTime::civilDateFromUtcParts(*year, *month, *day);
}

void applyMetadataLine(
    ILeapSecondProvider::TableInfo& info,
    const std::string_view line,
    const std::size_t lineNumber,
    std::optional<std::string>& diagnosticText
)
{
    if (EphemerisTableMetadata::applyCommonLine(info, line, kDataLabel, lineNumber, diagnosticText)) {
        return;
    }

    // The IANA list writes its expiration as a bare NTP timestamp after the
    // metadata marker.
    const std::optional<std::uint64_t> ntpExpiration =
        NumericParsingUtilities::parseUint64(line.substr(kMetadataMarkerSize));
    if (!ntpExpiration.has_value()) {
        diagnosticText = EphemerisTableMetadata::malformedMetadata(kDataLabel, "expiration", lineNumber);
        return;
    }

    info.expiresAt = skygate::core::EpochCodec::epochFromNtpTimestamp(*ntpExpiration);
}

[[nodiscard]] bool parseCsvEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitCommaSeparated(line);
    if (columns.size() != 2U) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        EphemerisEpochParsing::epochFromDateColumn(columns[0]);
    const std::optional<int> taiMinusUtcSeconds = NumericParsingUtilities::parseInt(columns[1]);
    if (!effectiveEpoch.has_value() || !taiMinusUtcSeconds.has_value()) {
        return false;
    }

    entry.effectiveUtcEpoch = *effectiveEpoch;
    entry.taiMinusUtcSeconds = *taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseIanaEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    const std::size_t commentOffset = line.find('#');
    const std::string_view payload = StringUtilities::trimAsciiWhitespace(
        commentOffset == std::string_view::npos ? line : line.substr(0U, commentOffset)
    );
    const std::vector<std::string_view> columns = TextParsingUtilities::splitAsciiWhitespace(payload);
    if (columns.size() < 2U) {
        return false;
    }

    const std::optional<std::uint64_t> ntpTimestamp = NumericParsingUtilities::parseUint64(columns[0]);
    const std::optional<int> taiMinusUtcSeconds = NumericParsingUtilities::parseInt(columns[1]);
    if (!ntpTimestamp.has_value() || !taiMinusUtcSeconds.has_value()) {
        return false;
    }

    const skygate::core::AstronomicalEpoch epoch = skygate::core::EpochCodec::epochFromNtpTimestamp(*ntpTimestamp);
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
    entry.taiMinusUtcSeconds = *taiMinusUtcSeconds;
    return true;
}

[[nodiscard]] bool parseEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    return parseCsvEntryLine(line, entry) || parseIanaEntryLine(line, entry);
}

}  // namespace

LeapSecondTableParser::Result LeapSecondTableParser::parse(const EphemerisTextDataAsset& asset)
{
    Result result;
    std::vector<ILeapSecondProvider::TableEntry> entries;
    if (std::optional<std::string> diagnosticText = EphemerisTableParsing::parsePayload(
            asset,
            result.tableInfo,
            entries,
            [&result](
                const std::string_view line,
                const std::size_t lineNumber,
                std::optional<std::string>& metadataDiagnostic
            ) { applyMetadataLine(result.tableInfo, line, lineNumber, metadataDiagnostic); },
            EphemerisTableParsing::skipNoLines,
            [](const std::string_view,
               const std::string_view trimmedLine,
               ILeapSecondProvider::TableEntry& entry) noexcept { return parseEntryLine(trimmedLine, entry); },
            kParseSpec,
            [](const ILeapSecondProvider::TableInfo&) noexcept { return std::optional<std::string>{}; }
        );
        diagnosticText.has_value()) {
        EphemerisTableParsing::markFailed(result.tableInfo, std::move(*diagnosticText));
        return result;
    }

    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
