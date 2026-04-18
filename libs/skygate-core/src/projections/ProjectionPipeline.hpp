#pragma once

#include "skygate/core/IProjection.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

namespace skygate::core {

class ProjectionPipeline final {
public:
    struct PreparedProjection {
        ProjectionParams params;
        ProjectionMath::Vec3 center;
        ProjectionMath::Vec3 right;
        ProjectionMath::Vec3 up;
        ProjectionMath::Vec3 target;
    };

    [[nodiscard]] static bool tryPrepare(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params,
        PreparedProjection& prepared,
        ScreenPoint& failurePoint
    ) noexcept;

    [[nodiscard]] static ScreenPoint culledPoint() noexcept;
    [[nodiscard]] static ScreenPoint invalidCoordinatePoint() noexcept;
    [[nodiscard]] static ScreenPoint invalidParametersPoint() noexcept;

    [[nodiscard]] static ScreenPoint finishCircular(
        double projectedX,
        double projectedY,
        const ProjectionParams& params,
        double maxRadius
    ) noexcept;

    [[nodiscard]] static ScreenPoint finishRectangular(
        double projectedX,
        double projectedY,
        const ProjectionParams& params,
        double halfWidth,
        double halfHeight
    ) noexcept;

private:
    [[nodiscard]] static ScreenPoint visiblePoint(double screenX, double screenY) noexcept;
};

}  // namespace skygate::core
