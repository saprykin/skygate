#pragma once

#include <cstdint>
#include <optional>

namespace skygate::core {

struct Vector2d final {
    double x = 0.0;
    double y = 0.0;
};

struct Rect2d final {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

struct LineSegment2d final {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
};

[[nodiscard]] double squaredDistance2d(
    double firstX,
    double firstY,
    double secondX,
    double secondY
) noexcept;

[[nodiscard]] double length2d(double x, double y) noexcept;
[[nodiscard]] Vector2d rotatedOffsetPoint2d(
    double originX,
    double originY,
    double offsetX,
    double offsetY,
    double rotationDeg
) noexcept;
[[nodiscard]] Vector2d ellipseOffsetPoint2d(
    double radiusX,
    double radiusY,
    int sampleIndex,
    int sampleCount
) noexcept;

[[nodiscard]] std::optional<Vector2d> perpendicularOffset2d(
    double startX,
    double startY,
    double endX,
    double endY,
    double halfWidth
) noexcept;

[[nodiscard]] std::int32_t gridCellIndex(double coordinate, double cellSize) noexcept;
[[nodiscard]] std::uint64_t packedGridCellKey(std::int32_t cellX, std::int32_t cellY) noexcept;
[[nodiscard]] std::uint64_t packedGridCellKey(double x, double y, double cellSize) noexcept;
[[nodiscard]] Vector2d gridCellCenter(double x, double y, double cellSize) noexcept;
[[nodiscard]] double areaScale(
    double width,
    double height,
    double referenceWidth,
    double referenceHeight
) noexcept;

[[nodiscard]] bool intersects(const Rect2d& lhs, const Rect2d& rhs) noexcept;
[[nodiscard]] bool fitsWithin(
    const Rect2d& rect,
    double width,
    double height,
    double margin
) noexcept;

}  // namespace skygate::core
