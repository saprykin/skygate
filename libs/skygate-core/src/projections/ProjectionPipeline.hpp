#pragma once

#include "skygate/core/ProjectionTypes.hpp"

namespace skygate::core {

class ProjectionPipeline final {
public:
    [[nodiscard]] static ScreenPoint culledPoint() noexcept;
    [[nodiscard]] static ScreenPoint invalidCoordinatePoint() noexcept;
    [[nodiscard]] static ScreenPoint invalidParametersPoint() noexcept;

    [[nodiscard]] static ScreenPoint finishCircular(
        double projectedX,
        double projectedY,
        const ProjectionParams& params,
        double maxRadius,
        double marginPx = 0.0
    ) noexcept;

    [[nodiscard]] static ScreenPoint finishRectangular(
        double projectedX,
        double projectedY,
        const ProjectionParams& params,
        double halfWidth,
        double halfHeight,
        double marginPx = 0.0
    ) noexcept;

private:
    [[nodiscard]] static ScreenPoint visiblePoint(double screenX, double screenY) noexcept;
};

}  // namespace skygate::core
