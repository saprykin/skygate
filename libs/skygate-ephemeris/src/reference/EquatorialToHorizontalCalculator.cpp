#include "EquatorialToHorizontalCalculator.hpp"
#include "math/AngleMath.hpp"
#include "math/SphericalGeometry.hpp"
#include "time/AstronomicalTime.hpp"

#include <cmath>

namespace skygate::ephemeris {

skygate::core::HorizontalCoordinate EquatorialToHorizontalCalculator::compute(
    const skygate::core::EquatorialCoordinate& equatorial,
    const skygate::core::GeoLocation& observer,
    const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    const double gmstDeg = skygate::core::AstronomicalTime::greenwichMeanSiderealTimeDeg(utcTime);
    const double localSiderealDeg = skygate::core::AngleMath::normalizeDegrees(gmstDeg + observer.longitudeDeg);
    const double hourAngleDeg = skygate::core::AngleMath::normalizeDegreesSigned(
        localSiderealDeg - skygate::core::AngleMath::hoursToDegrees(equatorial.rightAscensionHours)
    );

    const double hourAngleRad = skygate::core::AngleMath::toRadians(hourAngleDeg);
    const double declinationRad = skygate::core::AngleMath::toRadians(equatorial.declinationDeg);
    const double latitudeRad = skygate::core::AngleMath::toRadians(observer.latitudeDeg);

    const double x = std::cos(declinationRad) * std::cos(hourAngleRad);
    const double y = std::cos(declinationRad) * std::sin(hourAngleRad);
    const double z = std::sin(declinationRad);

    const double xHor = x * std::sin(latitudeRad) - z * std::cos(latitudeRad);
    const double yHor = y;
    const double zHor = x * std::cos(latitudeRad) + z * std::sin(latitudeRad);

    // xHor and yHor are the south and west components, so they are negated to
    // feed the north-toward-east azimuth convention.
    return skygate::core::SphericalGeometry::horizontalFromUnitVector({.x = -yHor, .y = -xHor, .z = zHor});
}

}  // namespace skygate::ephemeris
