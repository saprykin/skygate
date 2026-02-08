#include "skygate/core/projections/StereographicProjection.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

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
    ScreenPoint point;
    if (!ProjectionMath::areValidProjectionParams(params)) {
        return point;
    }

    ProjectionMath::Vec3 center;
    ProjectionMath::Vec3 right;
    ProjectionMath::Vec3 up;
    if (!ProjectionMath::tryBuildProjectionBasis(params.center, center, right, up)) {
        return point;
    }

    const ProjectionMath::Vec3 target = ProjectionMath::horizontalToUnitVector(coordinate);
    const double centerDotTarget = std::clamp(ProjectionMath::dot(target, center), -1.0, 1.0);
    const double denominator = 1.0 + centerDotTarget;
    if (denominator <= MathConstants::kEpsilon) {
        return point;
    }

    double projectedX = 2.0 * ProjectionMath::dot(target, right) / denominator;
    double projectedY = 2.0 * ProjectionMath::dot(target, up) / denominator;
    ProjectionMath::applyRoll(projectedX, projectedY, params.rollDeg);

    const double halfFovRad = ProjectionMath::toRadians(params.fovDeg) * 0.5;
    const double maxRadius = 2.0 * std::tan(halfFovRad * 0.5);
    if (maxRadius <= MathConstants::kEpsilon || !ProjectionMath::isFinite(maxRadius)) {
        return point;
    }

    if (std::hypot(projectedX, projectedY) > maxRadius) {
        return point;
    }

    const double scale = (0.5 * std::min(params.viewportWidth, params.viewportHeight)) / maxRadius;
    point.x = (0.5 * params.viewportWidth) + (projectedX * scale);
    point.y = (0.5 * params.viewportHeight) - (projectedY * scale);
    point.isVisible = ProjectionMath::isFinite(point.x) && ProjectionMath::isFinite(point.y);
    return point;
}

}  // namespace skygate::core
