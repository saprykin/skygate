#pragma once

#include "Rect2d.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace skygate::core {

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

}  // namespace skygate::core
