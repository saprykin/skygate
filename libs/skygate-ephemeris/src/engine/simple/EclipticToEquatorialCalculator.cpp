#include "EclipticToEquatorialCalculator.hpp"
#include "math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

skygate::core::EquatorialCoordinate EclipticToEquatorialCalculator::compute(
    const double eclipticLongitudeDeg, const double eclipticLatitudeDeg, const double obliquityDeg
) noexcept
{
    const double lonRad = skygate::core::AngleMath::toRadians(eclipticLongitudeDeg);
    const double latRad = skygate::core::AngleMath::toRadians(eclipticLatitudeDeg);
    const double epsRad = skygate::core::AngleMath::toRadians(obliquityDeg);

    const double xEcl = std::cos(lonRad) * std::cos(latRad);
    const double yEcl = std::sin(lonRad) * std::cos(latRad);
    const double zEcl = std::sin(latRad);

    const double xEq = xEcl;
    const double yEq = yEcl * std::cos(epsRad) - zEcl * std::sin(epsRad);
    const double zEq = yEcl * std::sin(epsRad) + zEcl * std::cos(epsRad);

    skygate::core::EquatorialCoordinate equatorial;
    equatorial.rightAscensionHours =
        skygate::core::AngleMath::normalizeHours(skygate::core::AngleMath::toDegrees(std::atan2(yEq, xEq)) / 15.0);
    equatorial.declinationDeg = skygate::core::AngleMath::toDegrees(std::asin(zEq));
    return equatorial;
}

}  // namespace skygate::ephemeris
