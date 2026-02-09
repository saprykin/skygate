#include "skygate/core/math/ViewportMath.hpp"

#include "skygate/core/math/AngleMath.hpp"

#include <algorithm>

namespace skygate::core {

double ViewportMath::normalizeAzimuthDeg(const double azimuthDeg) noexcept
{
    return AngleMath::normalizeDegrees(azimuthDeg);
}

double ViewportMath::clampAltitudeDeg(const double altitudeDeg) noexcept
{
    return std::clamp(altitudeDeg, kAltitudeMinDeg, kAltitudeMaxDeg);
}

double ViewportMath::clampFieldOfViewDeg(const double fieldOfViewDeg) noexcept
{
    return std::clamp(fieldOfViewDeg, kFieldOfViewMinDeg, kFieldOfViewMaxDeg);
}

ProjectionParams ViewportMath::buildProjectionParams(
    const double viewportWidth,
    const double viewportHeight,
    const double centerAltitudeDeg,
    const double centerAzimuthDeg,
    const double fieldOfViewDeg
) noexcept
{
    ProjectionParams projectionParams;
    projectionParams.center = {
        .altitudeDeg = centerAltitudeDeg,
        .azimuthDeg = centerAzimuthDeg
    };
    projectionParams.fovDeg = fieldOfViewDeg;
    projectionParams.rollDeg = 0.0;
    projectionParams.viewportWidth = viewportWidth;
    projectionParams.viewportHeight = viewportHeight;
    return projectionParams;
}

}  // namespace skygate::core
