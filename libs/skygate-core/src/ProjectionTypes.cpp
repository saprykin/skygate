#include "skygate/core/ProjectionTypes.hpp"

#include <cmath>

namespace skygate::core {

bool ScreenPoint::isFinite() const noexcept
{
    return std::isfinite(x) && std::isfinite(y);
}

bool ProjectionParams::isProjectable() const noexcept
{
    return center.isValid()
        && std::isfinite(fovDeg)
        && std::isfinite(rollDeg)
        && std::isfinite(viewportWidth)
        && std::isfinite(viewportHeight)
        && viewportWidth > 0.0
        && viewportHeight > 0.0
        && fovDeg >= kFieldOfViewMinDeg
        && fovDeg <= kFieldOfViewMaxDeg;
}

}  // namespace skygate::core
