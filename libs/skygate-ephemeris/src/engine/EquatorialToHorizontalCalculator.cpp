#include "engine/EquatorialToHorizontalCalculator.hpp"

#include "engine/AstronomicalTime.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

core::HorizontalCoordinate EquatorialToHorizontalCalculator::compute(
    const core::EquatorialCoordinate& equatorial,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    const double gmstDeg = AstronomicalTime::greenwichMeanSiderealTimeDeg(utcTime);
    const double localSiderealDeg = core::AngleMath::normalizeDegrees(gmstDeg + observer.longitudeDeg);
    const double hourAngleDeg = core::AngleMath::normalizeDegreesSigned(
        localSiderealDeg - equatorial.rightAscensionHours * 15.0
    );

    const double hourAngleRad = core::AngleMath::toRadians(hourAngleDeg);
    const double declinationRad = core::AngleMath::toRadians(equatorial.declinationDeg);
    const double latitudeRad = core::AngleMath::toRadians(observer.latitudeDeg);

    const double x = std::cos(declinationRad) * std::cos(hourAngleRad);
    const double y = std::cos(declinationRad) * std::sin(hourAngleRad);
    const double z = std::sin(declinationRad);

    const double xHor = x * std::sin(latitudeRad) - z * std::cos(latitudeRad);
    const double yHor = y;
    const double zHor = x * std::cos(latitudeRad) + z * std::sin(latitudeRad);

    core::HorizontalCoordinate horizontal;
    horizontal.altitudeDeg = core::AngleMath::toDegrees(std::asin(zHor));
    horizontal.azimuthDeg = core::AngleMath::normalizeDegrees(
        core::AngleMath::toDegrees(std::atan2(yHor, xHor)) + 180.0
    );
    return horizontal;
}

}  // namespace skygate::ephemeris
