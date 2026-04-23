#pragma once

#include <cstdint>
#include <optional>

namespace skygate::core {

struct Vector2d final {
    double x = 0.0;
    double y = 0.0;
};

class GeometryMath final {
public:
    [[nodiscard]] static double squaredDistance2d(
        double firstX,
        double firstY,
        double secondX,
        double secondY
    ) noexcept;

    [[nodiscard]] static double length2d(double x, double y) noexcept;
    [[nodiscard]] static Vector2d rotatedOffsetPoint2d(
        double originX,
        double originY,
        double offsetX,
        double offsetY,
        double rotationDeg
    ) noexcept;
    [[nodiscard]] static Vector2d ellipseOffsetPoint2d(
        double radiusX,
        double radiusY,
        int sampleIndex,
        int sampleCount
    ) noexcept;

    [[nodiscard]] static std::optional<Vector2d> perpendicularOffset2d(
        double startX,
        double startY,
        double endX,
        double endY,
        double halfWidth
    ) noexcept;

    [[nodiscard]] static std::int32_t gridCellIndex(double coordinate, double cellSize) noexcept;
    [[nodiscard]] static std::uint64_t packedGridCellKey(
        std::int32_t cellX,
        std::int32_t cellY
    ) noexcept;
    [[nodiscard]] static std::uint64_t packedGridCellKey(
        double x,
        double y,
        double cellSize
    ) noexcept;
    [[nodiscard]] static Vector2d gridCellCenter(double x, double y, double cellSize) noexcept;
    [[nodiscard]] static double areaScale(
        double width,
        double height,
        double referenceWidth,
        double referenceHeight
    ) noexcept;
};

}  // namespace skygate::core
