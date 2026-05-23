#include "HorizontalCoordinate.hpp"

#include "math/AngleMath.hpp"

#include <cmath>

namespace skygate::core {

bool HorizontalCoordinate::isFinite() const noexcept
{
    return std::isfinite(altitudeDeg) && std::isfinite(azimuthDeg);
}

bool HorizontalCoordinate::isValid() const noexcept
{
    return isFinite() && altitudeDeg >= kAltitudeMinDeg && altitudeDeg <= kAltitudeMaxDeg;
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
