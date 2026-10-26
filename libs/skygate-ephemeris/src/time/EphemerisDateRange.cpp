#include "EphemerisDateRange.hpp"

namespace skygate::ephemeris {

bool EphemerisDateRange::isValid() const noexcept
{
    return interval().isValid();
}

bool EphemerisDateRange::contains(const skygate::core::AstronomicalEpoch& epoch) const noexcept
{
    return interval().contains(epoch);
}

skygate::core::AstronomicalEpochRange EphemerisDateRange::interval() const noexcept
{
    return skygate::core::AstronomicalEpochRange{start, end};
}

}  // namespace skygate::ephemeris
