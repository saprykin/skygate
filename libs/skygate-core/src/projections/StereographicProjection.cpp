#include "skygate/core/projections/StereographicProjection.hpp"

#include "skygate/core/math/ProjectionMath.hpp"
#include "ProjectionPipeline.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

ProjectionType StereographicProjection::type() const noexcept
{
    return ProjectionType::Stereographic;
}

ScreenPoint StereographicProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    ProjectionPipeline::PreparedProjection prepared;
    ScreenPoint failurePoint;
    if (!ProjectionPipeline::tryPrepare(coordinate, params, prepared, failurePoint)) {
        return failurePoint;
    }

    const double centerDotTarget = std::clamp(
        ProjectionMath::dot(prepared.target, prepared.center),
        -1.0,
        1.0
    );
    const double denominator = 1.0 + centerDotTarget;
    if (denominator <= 0.0) {
        return ProjectionPipeline::culledPoint();
    }

    double projectedX = 2.0 * ProjectionMath::dot(prepared.target, prepared.right) / denominator;
    double projectedY = 2.0 * ProjectionMath::dot(prepared.target, prepared.up) / denominator;
    ProjectionMath::applyRoll(projectedX, projectedY, prepared.params.rollDeg);

    const double halfFovRad = ProjectionMath::toRadians(prepared.params.fovDeg) * 0.5;
    const double maxRadius = 2.0 * std::tan(halfFovRad * 0.5);
    return ProjectionPipeline::finishCircular(projectedX, projectedY, prepared.params, maxRadius);
}

}  // namespace skygate::core
