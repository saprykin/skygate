#include "RectOccupancyGrid.hpp"
#include "Geometry2d.hpp"
#include "GridCellBuckets.hpp"
#include "GridCellRange.hpp"

namespace skygate::core {

RectOccupancyGrid::RectOccupancyGrid(const double cellSize) : m_cellSize(cellSize) {}

bool RectOccupancyGrid::collides(const Rect2d& rect) const
{
    const GridCellRange cellRange = Geometry2d::gridCellRange(rect, m_cellSize);

    for (std::int32_t cellY = cellRange.minY; cellY <= cellRange.maxY; ++cellY) {
        for (std::int32_t cellX = cellRange.minX; cellX <= cellRange.maxX; ++cellX) {
            const std::vector<std::size_t>* rectIndices =
                GridCellBuckets::bucketAtCell(m_rectIndicesByCell, cellX, cellY);
            if (rectIndices == nullptr) {
                continue;
            }

            for (const std::size_t rectIndex : *rectIndices) {
                if (Geometry2d::intersects(rect, m_rects.at(rectIndex))) {
                    return true;
                }
            }
        }
    }

    return false;
}

void RectOccupancyGrid::add(const Rect2d& rect)
{
    const std::size_t rectIndex = m_rects.size();
    m_rects.push_back(rect);
    GridCellBuckets::insert(m_rectIndicesByCell, rect, m_cellSize, rectIndex);
}

void RectOccupancyGrid::clear()
{
    m_rects.clear();
    GridCellBuckets::clear(m_rectIndicesByCell);
}

}  // namespace skygate::core
