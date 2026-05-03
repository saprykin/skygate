#pragma once

#include "skygate/core/math/Geometry2d.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace skygate::core {

struct CircleHitTarget final {
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
    std::uint32_t payloadId = 0;
};

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

}  // namespace skygate::core
