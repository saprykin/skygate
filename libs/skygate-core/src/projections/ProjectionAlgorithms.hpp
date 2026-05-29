#pragma once

#include "HorizontalCoordinate.hpp"
#include "ProjectionParams.hpp"
#include "ProjectionType.hpp"
#include "ScreenPoint.hpp"
#include "math/Vector3d.hpp"

namespace skygate::core {

class ProjectionAlgorithms final {
public:
    struct ProjectionFrame final {
        ProjectionType projectionType = ProjectionType::Stereographic;
        ProjectionParams params;
        Vector3d center{};
        Vector3d right{};
        Vector3d up{};
        double circularMaxRadius = 0.0;
        double rectHalfWidth = 0.0;
        double rectHalfHeight = 0.0;
    };

    [[nodiscard]] static bool
    prepareFrame(ProjectionType projectionType, const ProjectionParams& params, ProjectionFrame& frame) noexcept;
    [[nodiscard]] static ScreenPoint
    project(const ProjectionFrame& frame, const HorizontalCoordinate& coordinate, double marginPx = 0.0) noexcept;
    [[nodiscard]] static ScreenPoint project(
        ProjectionType projectionType,
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params,
        double marginPx = 0.0
    ) noexcept;
};

}  // namespace skygate::core
