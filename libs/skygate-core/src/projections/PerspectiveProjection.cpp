#include "skygate/core/projections/PerspectiveProjection.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"
#include "ProjectionPipeline.hpp"

#include <cmath>

namespace skygate::core {

ProjectionType PerspectiveProjection::type() const noexcept
{
    return ProjectionType::Perspective;
}

ScreenPoint PerspectiveProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    ProjectionPipeline::PreparedProjection prepared;
    ScreenPoint failurePoint;
    if (!ProjectionPipeline::tryPrepare(coordinate, params, prepared, failurePoint)) {
        return failurePoint;
    }

    const double forward = ProjectionMath::dot(prepared.target, prepared.center);
    if (forward <= MathConstants::kEpsilon) {
        return ProjectionPipeline::culledPoint();
    }

    double projectedX = ProjectionMath::dot(prepared.target, prepared.right) / forward;
    double projectedY = ProjectionMath::dot(prepared.target, prepared.up) / forward;
    ProjectionMath::applyRoll(projectedX, projectedY, prepared.params.rollDeg);

    const double halfVerticalFovRad = ProjectionMath::toRadians(prepared.params.fovDeg) * 0.5;
    const double tanHalfVerticalFov = std::tan(halfVerticalFovRad);
    if (tanHalfVerticalFov <= MathConstants::kEpsilon || !ProjectionMath::isFinite(tanHalfVerticalFov)) {
        return ProjectionPipeline::invalidParametersPoint();
    }

    const double aspectRatio = prepared.params.viewportWidth / prepared.params.viewportHeight;
    const double tanHalfHorizontalFov = tanHalfVerticalFov * aspectRatio;
    if (tanHalfHorizontalFov <= MathConstants::kEpsilon || !ProjectionMath::isFinite(tanHalfHorizontalFov)) {
        return ProjectionPipeline::invalidParametersPoint();
    }

    return ProjectionPipeline::finishRectangular(
        projectedX,
        projectedY,
        prepared.params,
        tanHalfHorizontalFov,
        tanHalfVerticalFov
    );
}

}  // namespace skygate::core
