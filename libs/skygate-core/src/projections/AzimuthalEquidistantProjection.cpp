#include "skygate/core/projections/AzimuthalEquidistantProjection.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

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
    const double cosAngularDistance = std::clamp(ProjectionMath::dot(target, center), -1.0, 1.0);
    const double angularDistance = std::acos(cosAngularDistance);
    if (!ProjectionMath::isFinite(angularDistance)) {
        return point;
    }

    double projectedX = 0.0;
    double projectedY = 0.0;
    if (angularDistance > MathConstants::kEpsilon) {
        const double sinAngularDistance = std::sin(angularDistance);
        if (std::abs(sinAngularDistance) <= MathConstants::kEpsilon) {
            return point;
        }

        const double scale = angularDistance / sinAngularDistance;
        projectedX = scale * ProjectionMath::dot(target, right);
        projectedY = scale * ProjectionMath::dot(target, up);
    }

    ProjectionMath::applyRoll(projectedX, projectedY, params.rollDeg);

    const double maxRadius = ProjectionMath::toRadians(params.fovDeg) * 0.5;
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
