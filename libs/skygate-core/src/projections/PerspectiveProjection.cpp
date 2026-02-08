#include "skygate/core/projections/PerspectiveProjection.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

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
    const double forward = ProjectionMath::dot(target, center);
    if (forward <= MathConstants::kEpsilon) {
        return point;
    }

    double projectedX = ProjectionMath::dot(target, right) / forward;
    double projectedY = ProjectionMath::dot(target, up) / forward;
    ProjectionMath::applyRoll(projectedX, projectedY, params.rollDeg);

    const double halfVerticalFovRad = ProjectionMath::toRadians(params.fovDeg) * 0.5;
    const double tanHalfVerticalFov = std::tan(halfVerticalFovRad);
    if (tanHalfVerticalFov <= MathConstants::kEpsilon || !ProjectionMath::isFinite(tanHalfVerticalFov)) {
        return point;
    }

    const double aspectRatio = params.viewportWidth / params.viewportHeight;
    const double tanHalfHorizontalFov = tanHalfVerticalFov * aspectRatio;
    if (tanHalfHorizontalFov <= MathConstants::kEpsilon || !ProjectionMath::isFinite(tanHalfHorizontalFov)) {
        return point;
    }

    if (std::abs(projectedX) > tanHalfHorizontalFov || std::abs(projectedY) > tanHalfVerticalFov) {
        return point;
    }

    const double normalizedX = projectedX / tanHalfHorizontalFov;
    const double normalizedY = projectedY / tanHalfVerticalFov;
    point.x = ((normalizedX + 1.0) * 0.5) * params.viewportWidth;
    point.y = ((1.0 - normalizedY) * 0.5) * params.viewportHeight;
    point.isVisible = ProjectionMath::isFinite(point.x) && ProjectionMath::isFinite(point.y);
    return point;
}

}  // namespace skygate::core
