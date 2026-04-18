#include "skygate/core/SkyTypes.hpp"

#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::core {

bool GeoLocation::isValid() const noexcept
{
    return std::isfinite(latitudeDeg) && std::isfinite(longitudeDeg)
        && std::isfinite(elevationMeters)
        && latitudeDeg >= kLatitudeMinDeg && latitudeDeg <= kLatitudeMaxDeg
        && longitudeDeg >= kLongitudeMinDeg && longitudeDeg <= kLongitudeMaxDeg;
}

bool HorizontalCoordinate::isValid() const noexcept
{
    return std::isfinite(altitudeDeg)
        && std::isfinite(azimuthDeg)
        && altitudeDeg >= kAltitudeMinDeg
        && altitudeDeg <= kAltitudeMaxDeg;
}

HorizontalCoordinate HorizontalCoordinate::normalizedAzimuth() const noexcept
{
    HorizontalCoordinate coordinate = *this;
    if (std::isfinite(coordinate.azimuthDeg)) {
        coordinate.azimuthDeg = AngleMath::normalizeDegrees(coordinate.azimuthDeg);
    }
    return coordinate;
}

}  // namespace skygate::core
