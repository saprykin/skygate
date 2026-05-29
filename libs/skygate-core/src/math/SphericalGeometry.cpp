#include "SphericalGeometry.hpp"
#include "AngleMath.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

Vector3d SphericalGeometry::horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept
{
    const double altitudeRad = AngleMath::toRadians(coordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(coordinate.azimuthDeg);

    const double cosAltitude = std::cos(altitudeRad);
    return {
        .x = cosAltitude * std::sin(azimuthRad),
        .y = cosAltitude * std::cos(azimuthRad),
        .z = std::sin(altitudeRad),
    };
}

HorizontalCoordinate SphericalGeometry::horizontalFromUnitVector(const Vector3d& vector) noexcept
{
    return {
        .altitudeDeg = AngleMath::toDegrees(std::asin(std::clamp(vector.z, -1.0, 1.0))),
        .azimuthDeg = AngleMath::normalizeDegrees(AngleMath::toDegrees(std::atan2(vector.x, vector.y))),
    };
}

bool SphericalGeometry::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate, Vector3d& center, Vector3d& right, Vector3d& up
) noexcept
{
    if (!centerCoordinate.isValid()) {
        return false;
    }

    const HorizontalCoordinate normalizedCoordinate = centerCoordinate.normalizedAzimuth();
    const double altitudeRad = AngleMath::toRadians(normalizedCoordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(normalizedCoordinate.azimuthDeg);

    const double sinAltitude = std::sin(altitudeRad);
    const double cosAltitude = std::cos(altitudeRad);
    const double sinAzimuth = std::sin(azimuthRad);
    const double cosAzimuth = std::cos(azimuthRad);

    center = {.x = cosAltitude * sinAzimuth, .y = cosAltitude * cosAzimuth, .z = sinAltitude};
    right = {.x = -cosAzimuth, .y = sinAzimuth, .z = 0.0};
    up = {.x = -sinAltitude * sinAzimuth, .y = -sinAltitude * cosAzimuth, .z = cosAltitude};
    return true;
}

}  // namespace skygate::core
