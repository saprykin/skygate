#include "LeapSecondTableParser.hpp"
#include "EphemerisTableMetadata.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "NumericParsingUtilities.hpp"
#include "StringUtilities.hpp"
#include "TextParsingUtilities.hpp"
#include "time/CalendarTime.hpp"
#include "time/EpochCodec.hpp"

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

[[nodiscard]] std::optional<std::string>
applyMetadataLine(ILeapSecondProvider::TableInfo& info, const std::string_view line, const std::size_t lineNumber)
{
    std::optional<std::string> diagnosticText;
    if (EphemerisTableMetadata::applyCommonLine(info, line, "Leap-second table", lineNumber, diagnosticText)) {
        return diagnosticText;
    }
    if (line.starts_with("#@")) {
        const std::optional<std::uint64_t> ntpExpiration = NumericParsingUtilities::parseUint64(line.substr(2U));
        if (!ntpExpiration.has_value()) {
            return "Leap-second table contains malformed expiration metadata at line " + std::to_string(lineNumber)
                   + ".";
        }

        info.expiresAt = skygate::core::EpochCodec::epochFromNtpTimestamp(*ntpExpiration);
    }

    return std::nullopt;
}

[[nodiscard]] bool parseCsvEntryLine(std::string_view line, ILeapSecondProvider::TableEntry& entry) noexcept
{
    const std::vector<std::string_view> columns = TextParsingUtilities::splitCommaSeparated(line);
    if (columns.size() != 2U) {
        return false;
    }

    const std::optional<skygate::core::CivilDateTime> effectiveDate = TextParsingUtilities::parseUtcDate(columns[0]);
    const std::optional<int> taiMinusUtcSeconds = NumericParsingUtilities::parseInt(columns[1]);
    if (!effectiveDate.has_value() || !taiMinusUtcSeconds.has_value()) {
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> effectiveEpoch =
        skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*effectiveDate);
    if (!effectiveEpoch.has_value()) {
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
        std::string_view line = StringUtilities::takeLine(remaining);

        line = StringUtilities::trimAsciiWhitespace(line);
        if (line.empty()) {
            continue;
        }
        if (line.starts_with("#@")) {
            if (std::optional<std::string> diagnosticText = applyMetadataLine(info, line, lineNumber);
                diagnosticText.has_value()) {
                return failureResult(
                    std::move(info), ILeapSecondProvider::TableStatus::Malformed, std::move(*diagnosticText)
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

    info.validityRange = EphemerisTableMetadata::makeRange(
        kValidityRangeId,
        kValidityRangeDisplayName,
        entries.front().effectiveUtcEpoch,
        info.expiresAt.value_or(entries.back().effectiveUtcEpoch)
    );
    info.status = ILeapSecondProvider::TableStatus::Available;
    info.diagnosticText = "Leap-second table loaded.";

    LeapSecondTableParser::Result result;
    result.tableInfo = std::move(info);
    result.entries = std::move(entries);
    return result;
}

}  // namespace skygate::ephemeris
