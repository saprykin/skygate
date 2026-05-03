#include "skygate/core/math/GeometryMath.hpp"

namespace skygate::core {

double GeometryMath::squaredDistance2d(
    const double firstX,
    const double firstY,
    const double secondX,
    const double secondY
) noexcept
{
    return skygate::core::squaredDistance2d(firstX, firstY, secondX, secondY);
}

double GeometryMath::length2d(const double x, const double y) noexcept
{
    return skygate::core::length2d(x, y);
}

Vector2d GeometryMath::rotatedOffsetPoint2d(
    const double originX,
    const double originY,
    const double offsetX,
    const double offsetY,
    const double rotationDeg
) noexcept
{
    return skygate::core::rotatedOffsetPoint2d(
        originX,
        originY,
        offsetX,
        offsetY,
        rotationDeg
    );
}

Vector2d GeometryMath::ellipseOffsetPoint2d(
    const double radiusX,
    const double radiusY,
    const int sampleIndex,
    const int sampleCount
) noexcept
{
    return skygate::core::ellipseOffsetPoint2d(radiusX, radiusY, sampleIndex, sampleCount);
}

std::optional<Vector2d> GeometryMath::perpendicularOffset2d(
    const double startX,
    const double startY,
    const double endX,
    const double endY,
    const double halfWidth
) noexcept
{
    return skygate::core::perpendicularOffset2d(startX, startY, endX, endY, halfWidth);
}

std::int32_t GeometryMath::gridCellIndex(
    const double coordinate,
    const double cellSize
) noexcept
{
    return skygate::core::gridCellIndex(coordinate, cellSize);
}

std::uint64_t GeometryMath::packedGridCellKey(
    const std::int32_t cellX,
    const std::int32_t cellY
) noexcept
{
    return skygate::core::packedGridCellKey(cellX, cellY);
}

std::uint64_t GeometryMath::packedGridCellKey(
    const double x,
    const double y,
    const double cellSize
) noexcept
{
    return skygate::core::packedGridCellKey(x, y, cellSize);
}

Vector2d GeometryMath::gridCellCenter(
    const double x,
    const double y,
    const double cellSize
) noexcept
{
    return skygate::core::gridCellCenter(x, y, cellSize);
}

double GeometryMath::areaScale(
    const double width,
    const double height,
    const double referenceWidth,
    const double referenceHeight
) noexcept
{
    return skygate::core::areaScale(width, height, referenceWidth, referenceHeight);
}

}  // namespace skygate::core
