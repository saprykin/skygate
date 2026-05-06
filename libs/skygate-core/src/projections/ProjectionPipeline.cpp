#include "ProjectionPipeline.hpp"

#include "skygate/core/math/MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

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
    const double maxRadius,
    const double marginPx
) noexcept
{
    if (maxRadius <= MathConstants::kEpsilon || !std::isfinite(maxRadius)) {
        return invalidParametersPoint();
    }

    const double scale = (0.5 * std::min(params.viewportWidth, params.viewportHeight)) / maxRadius;
    if (!std::isfinite(scale) || scale <= MathConstants::kEpsilon) {
        return invalidParametersPoint();
    }

    const double boundedMarginPx = std::max(0.0, marginPx);
    if (std::hypot(projectedX, projectedY) > (maxRadius + (boundedMarginPx / scale))) {
        return culledPoint();
    }

    const double screenX = (0.5 * params.viewportWidth) + (projectedX * scale);
    const double screenY = (0.5 * params.viewportHeight) - (projectedY * scale);
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return invalidParametersPoint();
    }

    return visiblePoint(screenX, screenY);
}

ScreenPoint ProjectionPipeline::finishRectangular(
    const double projectedX,
    const double projectedY,
    const ProjectionParams& params,
    const double halfWidth,
    const double halfHeight,
    const double marginPx
) noexcept
{
    if (halfWidth <= MathConstants::kEpsilon || halfHeight <= MathConstants::kEpsilon) {
        return invalidParametersPoint();
    }

    if (!std::isfinite(halfWidth) || !std::isfinite(halfHeight)) {
        return invalidParametersPoint();
    }

    const double scaleX = params.viewportWidth / (halfWidth * 2.0);
    const double scaleY = params.viewportHeight / (halfHeight * 2.0);
    if (
        !std::isfinite(scaleX)
        || !std::isfinite(scaleY)
        || scaleX <= MathConstants::kEpsilon
        || scaleY <= MathConstants::kEpsilon
    ) {
        return invalidParametersPoint();
    }

    const double boundedMarginPx = std::max(0.0, marginPx);
    const double marginX = boundedMarginPx / scaleX;
    const double marginY = boundedMarginPx / scaleY;
    if (
        std::abs(projectedX) > (halfWidth + marginX + MathConstants::kEpsilon)
        || std::abs(projectedY) > (halfHeight + marginY + MathConstants::kEpsilon)
    ) {
        return culledPoint();
    }

    const double normalizedX = projectedX / halfWidth;
    const double normalizedY = projectedY / halfHeight;
    const double screenX = ((normalizedX + 1.0) * 0.5) * params.viewportWidth;
    const double screenY = ((1.0 - normalizedY) * 0.5) * params.viewportHeight;
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
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
