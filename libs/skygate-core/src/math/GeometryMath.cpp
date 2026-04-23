#include "skygate/core/math/GeometryMath.hpp"

#include <cmath>

namespace skygate::core {

double GeometryMath::squaredDistance2d(
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

double GeometryMath::length2d(const double x, const double y) noexcept
{
    return std::hypot(x, y);
}

std::optional<Vector2d> GeometryMath::perpendicularOffset2d(
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

std::int32_t GeometryMath::gridCellIndex(
    const double coordinate,
    const double cellSize
) noexcept
{
    if (!std::isfinite(coordinate) || !std::isfinite(cellSize) || cellSize <= 0.0) {
        return 0;
    }

    return static_cast<std::int32_t>(std::floor(coordinate / cellSize));
}

std::uint64_t GeometryMath::packedGridCellKey(
    const std::int32_t cellX,
    const std::int32_t cellY
) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX)) << 32U)
        | static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellY));
}

std::uint64_t GeometryMath::packedGridCellKey(
    const double x,
    const double y,
    const double cellSize
) noexcept
{
    return packedGridCellKey(gridCellIndex(x, cellSize), gridCellIndex(y, cellSize));
}

Vector2d GeometryMath::gridCellCenter(
    const double x,
    const double y,
    const double cellSize
) noexcept
{
    const std::int32_t cellX = gridCellIndex(x, cellSize);
    const std::int32_t cellY = gridCellIndex(y, cellSize);
    return Vector2d {
        .x = (static_cast<double>(cellX) * cellSize) + (cellSize * 0.5),
        .y = (static_cast<double>(cellY) * cellSize) + (cellSize * 0.5)
    };
}

double GeometryMath::areaScale(
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

}  // namespace skygate::core
