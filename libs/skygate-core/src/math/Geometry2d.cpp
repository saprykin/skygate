#include "skygate/core/math/Geometry2d.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/MathConstants.hpp"

#include <cmath>

namespace skygate::core {

double squaredDistance2d(
    const double firstX,
    const double firstY,
    const double secondX,
    const double secondY
) noexcept
{
    const double deltaX = firstX - secondX;
    const double deltaY = firstY - secondY;
    return (deltaX * deltaX) + (deltaY * deltaY);
}

double length2d(const double x, const double y) noexcept
{
    return std::hypot(x, y);
}

Vector2d rotatedOffsetPoint2d(
    const double originX,
    const double originY,
    const double offsetX,
    const double offsetY,
    const double rotationDeg
) noexcept
{
    const double rotationRadians = AngleMath::toRadians(rotationDeg);
    const double cosRotation = std::cos(rotationRadians);
    const double sinRotation = std::sin(rotationRadians);
    return Vector2d {
        .x = originX + (offsetX * cosRotation) - (offsetY * sinRotation),
        .y = originY + (offsetX * sinRotation) + (offsetY * cosRotation)
    };
}

Vector2d ellipseOffsetPoint2d(
    const double radiusX,
    const double radiusY,
    const int sampleIndex,
    const int sampleCount
) noexcept
{
    if (sampleCount <= 0) {
        return Vector2d {
            .x = radiusX,
            .y = 0.0
        };
    }

    const double angle = (2.0 * MathConstants::kPi * static_cast<double>(sampleIndex))
        / static_cast<double>(sampleCount);
    return Vector2d {
        .x = std::cos(angle) * radiusX,
        .y = std::sin(angle) * radiusY
    };
}

std::optional<Vector2d> perpendicularOffset2d(
    const double startX,
    const double startY,
    const double endX,
    const double endY,
    const double halfWidth
) noexcept
{
    const double deltaX = endX - startX;
    const double deltaY = endY - startY;
    const double length = length2d(deltaX, deltaY);
    if (length <= 0.0 || !std::isfinite(length)) {
        return std::nullopt;
    }

    return Vector2d {
        .x = -deltaY / length * halfWidth,
        .y = deltaX / length * halfWidth
    };
}

std::int32_t gridCellIndex(const double coordinate, const double cellSize) noexcept
{
    if (!std::isfinite(coordinate) || !std::isfinite(cellSize) || cellSize <= 0.0) {
        return 0;
    }

    return static_cast<std::int32_t>(std::floor(coordinate / cellSize));
}

std::uint64_t packedGridCellKey(
    const std::int32_t cellX,
    const std::int32_t cellY
) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX)) << 32U)
        | static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellY));
}

std::uint64_t packedGridCellKey(
    const double x,
    const double y,
    const double cellSize
) noexcept
{
    return packedGridCellKey(gridCellIndex(x, cellSize), gridCellIndex(y, cellSize));
}

Vector2d gridCellCenter(const double x, const double y, const double cellSize) noexcept
{
    const std::int32_t cellX = gridCellIndex(x, cellSize);
    const std::int32_t cellY = gridCellIndex(y, cellSize);
    return Vector2d {
        .x = (static_cast<double>(cellX) * cellSize) + (cellSize * 0.5),
        .y = (static_cast<double>(cellY) * cellSize) + (cellSize * 0.5)
    };
}

double areaScale(
    const double width,
    const double height,
    const double referenceWidth,
    const double referenceHeight
) noexcept
{
    const double referenceArea = referenceWidth * referenceHeight;
    if (
        !std::isfinite(width)
        || !std::isfinite(height)
        || !std::isfinite(referenceArea)
        || width <= 0.0
        || height <= 0.0
        || referenceArea <= 0.0
    ) {
        return 1.0;
    }

    return std::sqrt((width * height) / referenceArea);
}

bool intersects(const Rect2d& lhs, const Rect2d& rhs) noexcept
{
    return lhs.left < rhs.right
        && lhs.right > rhs.left
        && lhs.top < rhs.bottom
        && lhs.bottom > rhs.top;
}

bool fitsWithin(
    const Rect2d& rect,
    const double width,
    const double height,
    const double margin
) noexcept
{
    return rect.left >= margin
        && rect.right <= (width - margin)
        && rect.top >= margin
        && rect.bottom <= (height - margin);
}

}  // namespace skygate::core
