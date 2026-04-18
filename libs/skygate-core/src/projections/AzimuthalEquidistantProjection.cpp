#include "skygate/core/projections/AzimuthalEquidistantProjection.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"
#include "ProjectionPipeline.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

ProjectionType AzimuthalEquidistantProjection::type() const noexcept
{
    return ProjectionType::AzimuthalEquidistant;
}

ScreenPoint AzimuthalEquidistantProjection::project(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params
) const noexcept
{
    ProjectionPipeline::PreparedProjection prepared;
    ScreenPoint failurePoint;
    if (!ProjectionPipeline::tryPrepare(coordinate, params, prepared, failurePoint)) {
        return failurePoint;
    }

    const double cosAngularDistance = std::clamp(
        ProjectionMath::dot(prepared.target, prepared.center),
        -1.0,
        1.0
    );
    const double angularDistance = std::acos(cosAngularDistance);
    if (!ProjectionMath::isFinite(angularDistance)) {
        return ProjectionPipeline::invalidParametersPoint();
    }

    double projectedX = 0.0;
    double projectedY = 0.0;
    if (angularDistance > MathConstants::kEpsilon) {
        const double sinAngularDistance = std::sin(angularDistance);
        if (std::abs(sinAngularDistance) <= MathConstants::kEpsilon) {
            return ProjectionPipeline::culledPoint();
        }

        const double scale = angularDistance / sinAngularDistance;
        projectedX = scale * ProjectionMath::dot(prepared.target, prepared.right);
        projectedY = scale * ProjectionMath::dot(prepared.target, prepared.up);
    }

    ProjectionMath::applyRoll(projectedX, projectedY, prepared.params.rollDeg);

    const double maxRadius = ProjectionMath::toRadians(prepared.params.fovDeg) * 0.5;
    return ProjectionPipeline::finishCircular(projectedX, projectedY, prepared.params, maxRadius);
}

}  // namespace skygate::core
