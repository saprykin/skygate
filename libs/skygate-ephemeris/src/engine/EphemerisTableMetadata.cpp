#include "EphemerisTableMetadata.hpp"
#include "StringUtilities.hpp"
#include "time/CalendarTime.hpp"

#include <optional>

namespace skygate::ephemeris {

bool EphemerisTableMetadata::isHeaderLine(const std::string_view line) noexcept
{
    return line.starts_with("effective_utc_date");
}

bool EphemerisTableMetadata::isMetadataLine(const std::string_view line) noexcept
{
    // A metadata line is the marker of kMetadataLinePrefix followed by an
    // ASCII whitespace separator. The canonical separator is a space; the
    // IANA leap-second list writes a tab before its expiration stamp.
    constexpr std::string_view kMarker = kMetadataLinePrefix.substr(0U, kMetadataLinePrefix.size() - 1U);
    return line.starts_with(kMarker)
           && (line.size() == kMarker.size() || StringUtilities::isAsciiWhitespace(line[kMarker.size()]));
}

EphemerisDateRange EphemerisTableMetadata::makeRange(
    const std::string_view id,
    const std::string_view displayName,
    const skygate::core::AstronomicalEpoch& start,
    const skygate::core::AstronomicalEpoch& end
)
{
    EphemerisDateRange range;
    range.id = id;
    range.displayName = displayName;
    range.start = start;
    range.end = end;
    return range;
}

std::string EphemerisTableMetadata::malformedMetadata(
    const std::string_view dataLabel, const std::string_view subject, const std::size_t lineNumber
)
{
    return std::string(dataLabel) + " contains malformed " + std::string(subject) + " metadata at line "
           + std::to_string(lineNumber) + ".";
}

std::optional<std::string> EphemerisTableMetadata::assignRangeEndpoint(
    EphemerisDateRange& range,
    skygate::core::AstronomicalEpoch EphemerisDateRange::* const endpoint,
    const std::string_view value,
    const std::string_view rangeId,
    const std::string_view rangeDisplayName,
    const std::string_view dataLabel,
    const std::string_view subject,
    const std::size_t lineNumber,
    bool& hasEndpoint
)
{
    const std::optional<skygate::core::CivilDateTime> date = TextParsingUtilities::parseUtcDate(value);
    const std::optional<skygate::core::AstronomicalEpoch> epoch =
        date.has_value() ? skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(*date) : std::nullopt;
    if (!epoch.has_value()) {
        return malformedMetadata(dataLabel, subject, lineNumber);
    }

    range.id = rangeId;
    range.displayName = rangeDisplayName;
    range.*endpoint = *epoch;
    hasEndpoint = true;
    return std::nullopt;
}

}  // namespace skygate::ephemeris
