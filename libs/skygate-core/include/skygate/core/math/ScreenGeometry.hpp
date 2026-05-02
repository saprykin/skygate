#pragma once

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionTypes.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace skygate::core {

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

struct CircleHitTarget final {
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
    std::uint32_t payloadId = 0;
};

[[nodiscard]] bool intersects(const Rect2d& lhs, const Rect2d& rhs) noexcept;
[[nodiscard]] bool fitsWithin(
    const Rect2d& rect,
    double width,
    double height,
    double margin
) noexcept;

class RectOccupancyGrid final {
public:
    explicit RectOccupancyGrid(double cellSize);

    [[nodiscard]] bool collides(const Rect2d& rect) const;
    void add(const Rect2d& rect);
    void clear();

private:
    double m_cellSize = 64.0;
    std::vector<Rect2d> m_rects;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_rectIndicesByCell;
};

class CircleHitIndex final {
public:
    explicit CircleHitIndex(double cellSize);

    void rebuild(std::span<const CircleHitTarget> targets);
    void clear();
    [[nodiscard]] std::optional<std::uint32_t> nearestPayloadAt(double x, double y) const;

private:
    double m_cellSize = 24.0;
    std::vector<CircleHitTarget> m_targets;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_targetIndicesByCell;
};

class ProjectedPolylineBuilder final {
public:
    [[nodiscard]] std::vector<LineSegment2d> build(
        const PreparedProjection& projection,
        std::span<const HorizontalCoordinate> coordinates,
        double maxSegmentLengthSquared
    ) const;
};

class DashedLineBuilder final {
public:
    [[nodiscard]] std::vector<LineSegment2d> build(
        const LineSegment2d& segment,
        double dashLength,
        double gapLength
    ) const;
};

}  // namespace skygate::core
