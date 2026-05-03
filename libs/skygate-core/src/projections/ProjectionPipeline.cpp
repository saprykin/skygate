#include "ProjectionPipeline.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

bool ProjectionPipeline::tryPrepare(
    const HorizontalCoordinate& coordinate,
    const ProjectionParams& params,
    ProjectionPreparation& prepared,
    ScreenPoint& failurePoint
) noexcept
{
    if (!params.isProjectable()) {
        failurePoint = invalidParametersPoint();
        return false;
    }

    if (!coordinate.isValid()) {
        failurePoint = invalidCoordinatePoint();
        return false;
    }

    prepared.params = params;
    prepared.params.center = params.center.normalizedAzimuth();

    if (!SphericalGeometry::tryBuildProjectionBasis(
            prepared.params.center,
            prepared.center,
            prepared.right,
            prepared.up
        )) {
        failurePoint = invalidParametersPoint();
        return false;
    }

    prepared.target = SphericalGeometry::horizontalToUnitVector(coordinate.normalizedAzimuth());
    failurePoint = culledPoint();
    return true;
}

ScreenPoint ProjectionPipeline::culledPoint() noexcept
{
    return {};
}

ScreenPoint ProjectionPipeline::invalidCoordinatePoint() noexcept
{
    ScreenPoint point;
    point.status = ProjectionStatus::InvalidCoordinate;
    return point;
}

ScreenPoint ProjectionPipeline::invalidParametersPoint() noexcept
{
    ScreenPoint point;
    point.status = ProjectionStatus::InvalidParameters;
    return point;
}

ScreenPoint ProjectionPipeline::finishCircular(
    const double projectedX,
    const double projectedY,
    const ProjectionParams& params,
    const double maxRadius
) noexcept
{
    if (maxRadius <= MathConstants::kEpsilon || !ProjectionMath::isFinite(maxRadius)) {
        return invalidParametersPoint();
    }

    if (std::hypot(projectedX, projectedY) > maxRadius) {
        return culledPoint();
    }

    const double scale = (0.5 * std::min(params.viewportWidth, params.viewportHeight)) / maxRadius;
    if (!ProjectionMath::isFinite(scale) || scale <= MathConstants::kEpsilon) {
        return invalidParametersPoint();
    }

    const double screenX = (0.5 * params.viewportWidth) + (projectedX * scale);
    const double screenY = (0.5 * params.viewportHeight) - (projectedY * scale);
    if (!ProjectionMath::isFinite(screenX) || !ProjectionMath::isFinite(screenY)) {
        return invalidParametersPoint();
    }

    return visiblePoint(screenX, screenY);
}

ScreenPoint ProjectionPipeline::finishRectangular(
    const double projectedX,
    const double projectedY,
    const ProjectionParams& params,
    const double halfWidth,
    const double halfHeight
) noexcept
{
    if (halfWidth <= MathConstants::kEpsilon || halfHeight <= MathConstants::kEpsilon) {
        return invalidParametersPoint();
    }

    if (!ProjectionMath::isFinite(halfWidth) || !ProjectionMath::isFinite(halfHeight)) {
        return invalidParametersPoint();
    }

    if (std::abs(projectedX) > halfWidth || std::abs(projectedY) > halfHeight) {
        return culledPoint();
    }

    const double normalizedX = projectedX / halfWidth;
    const double normalizedY = projectedY / halfHeight;
    const double screenX = ((normalizedX + 1.0) * 0.5) * params.viewportWidth;
    const double screenY = ((1.0 - normalizedY) * 0.5) * params.viewportHeight;
    if (!ProjectionMath::isFinite(screenX) || !ProjectionMath::isFinite(screenY)) {
        return invalidParametersPoint();
    }

    return visiblePoint(screenX, screenY);
}

ScreenPoint ProjectionPipeline::visiblePoint(const double screenX, const double screenY) noexcept
{
    ScreenPoint point;
    point.x = screenX;
    point.y = screenY;
    point.isVisible = true;
    point.status = ProjectionStatus::Visible;
    return point;
}

}  // namespace skygate::core
