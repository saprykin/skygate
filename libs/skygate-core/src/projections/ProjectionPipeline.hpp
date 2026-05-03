#pragma once

#include "skygate/core/IProjection.hpp"
#include "skygate/core/math/SphericalGeometry.hpp"

namespace skygate::core {

class ProjectionPipeline final {
public:
    struct ProjectionPreparation final {
        ProjectionParams params;
        SphericalGeometry::Vector3d center;
        SphericalGeometry::Vector3d right;
        SphericalGeometry::Vector3d up;
        SphericalGeometry::Vector3d target;
    };

    [[nodiscard]] static bool tryPrepare(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params,
        ProjectionPreparation& prepared,
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
