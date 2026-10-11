#include "FixedBodyAltitudeRange.hpp"

#include <cmath>

namespace skygate::ephemeris {

double FixedBodyAltitudeRange::minAltitudeDeg(const double observerLatitudeDeg, const double declinationDeg) noexcept
{
    return std::abs(observerLatitudeDeg + declinationDeg) - 90.0;
}

double FixedBodyAltitudeRange::maxAltitudeDeg(const double observerLatitudeDeg, const double declinationDeg) noexcept
{
    return 90.0 - std::abs(observerLatitudeDeg - declinationDeg);
}

}  // namespace skygate::ephemeris
