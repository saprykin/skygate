#include "EphemerisTableMetadata.hpp"

namespace skygate::ephemeris {

bool EphemerisTableMetadata::isHeaderLine(const std::string_view line) noexcept
{
    return line.starts_with("effective_utc_date");
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

}  // namespace skygate::ephemeris
