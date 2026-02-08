#include "engine/CoordinateTransform.hpp"

#include "engine/JulianDateTime.hpp"
#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

core::EquatorialCoordinate CoordinateTransform::eclipticToEquatorial(
    const double eclipticLongitudeDeg,
    const double eclipticLatitudeDeg,
    const double obliquityDeg
) noexcept
{
    const double lonRad = core::AngleMath::toRadians(eclipticLongitudeDeg);
    const double latRad = core::AngleMath::toRadians(eclipticLatitudeDeg);
    const double epsRad = core::AngleMath::toRadians(obliquityDeg);

    const double xEcl = std::cos(lonRad) * std::cos(latRad);
    const double yEcl = std::sin(lonRad) * std::cos(latRad);
    const double zEcl = std::sin(latRad);

    const double xEq = xEcl;
    const double yEq = yEcl * std::cos(epsRad) - zEcl * std::sin(epsRad);
    const double zEq = yEcl * std::sin(epsRad) + zEcl * std::cos(epsRad);

    core::EquatorialCoordinate equatorial;
    equatorial.rightAscensionHours = core::AngleMath::normalizeHours(
        core::AngleMath::toDegrees(std::atan2(yEq, xEq)) / 15.0
    );
    equatorial.declinationDeg = core::AngleMath::toDegrees(std::asin(zEq));
    return equatorial;
}

core::HorizontalCoordinate CoordinateTransform::equatorialToHorizontal(
    const core::EquatorialCoordinate& equatorial,
    const core::GeoLocation& observer,
    const core::UtcTimePoint& utcTime
) noexcept
{
    const double gmstDeg = JulianDateTime::greenwichMeanSiderealTimeDeg(utcTime);
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
