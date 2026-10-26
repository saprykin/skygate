#include "ProjectionPipeline.hpp"
#include "math/MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {

ScreenPoint ProjectionPipeline::finishCircular(
    const double projectedX,
    const double projectedY,
    const ProjectionParams& params,
    const double maxRadius,
    const double marginPx
) noexcept
{
    if (maxRadius <= MathConstants::kEpsilon || !std::isfinite(maxRadius)) {
        return ScreenPoint::invalidParameters();
    }

    const double scale = (0.5 * std::min(params.viewportWidth, params.viewportHeight)) / maxRadius;
    if (!std::isfinite(scale) || scale <= MathConstants::kEpsilon) {
        return ScreenPoint::invalidParameters();
    }

    const double boundedMarginPx = std::max(0.0, marginPx);
    if (std::hypot(projectedX, projectedY) > (maxRadius + (boundedMarginPx / scale))) {
        return ScreenPoint::culled();
    }

    const double screenX = (0.5 * params.viewportWidth) + (projectedX * scale);
    const double screenY = (0.5 * params.viewportHeight) - (projectedY * scale);
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return ScreenPoint::invalidParameters();
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
        return ScreenPoint::invalidParameters();
    }

    if (!std::isfinite(halfWidth) || !std::isfinite(halfHeight)) {
        return ScreenPoint::invalidParameters();
    }

    const double scaleX = params.viewportWidth / (halfWidth * 2.0);
    const double scaleY = params.viewportHeight / (halfHeight * 2.0);
    if (!std::isfinite(scaleX) || !std::isfinite(scaleY) || scaleX <= MathConstants::kEpsilon
        || scaleY <= MathConstants::kEpsilon) {
        return ScreenPoint::invalidParameters();
    }

    const double boundedMarginPx = std::max(0.0, marginPx);
    const double marginX = boundedMarginPx / scaleX;
    const double marginY = boundedMarginPx / scaleY;
    if (std::abs(projectedX) > (halfWidth + marginX + MathConstants::kEpsilon)
        || std::abs(projectedY) > (halfHeight + marginY + MathConstants::kEpsilon)) {
        return ScreenPoint::culled();
    }

    const double normalizedX = projectedX / halfWidth;
    const double normalizedY = projectedY / halfHeight;
    const double screenX = ((normalizedX + 1.0) * 0.5) * params.viewportWidth;
    const double screenY = ((1.0 - normalizedY) * 0.5) * params.viewportHeight;
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return ScreenPoint::invalidParameters();
    }

    return visiblePoint(screenX, screenY);
}

ScreenPoint ProjectionPipeline::visiblePoint(const double screenX, const double screenY) noexcept
{
    ScreenPoint point;
    point.x = screenX;
    point.y = screenY;
    point.status = ProjectionStatus::Visible;
    return point;
}

}  // namespace skygate::core
