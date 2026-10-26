#pragma once

#include "GridCellBuckets.hpp"
#include "Rect2d.hpp"

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
    GridCellBuckets::CellIndices m_rectIndicesByCell;
};

}  // namespace skygate::core
