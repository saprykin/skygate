#include "RectOccupancyGrid.hpp"
#include "Geometry2d.hpp"
#include "GridCellRange.hpp"

namespace skygate::core {

RectOccupancyGrid::RectOccupancyGrid(const double cellSize) : m_cellSize(cellSize) {}

bool RectOccupancyGrid::collides(const Rect2d& rect) const
{
    const GridCellRange cellRange = Geometry2d::gridCellRange(rect, m_cellSize);

    for (std::int32_t cellY = cellRange.minY; cellY <= cellRange.maxY; ++cellY) {
        for (std::int32_t cellX = cellRange.minX; cellX <= cellRange.maxX; ++cellX) {
            const auto cellIt = m_rectIndicesByCell.find(Geometry2d::packedGridCellKey(cellX, cellY));
            if (cellIt == m_rectIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t rectIndex : cellIt->second) {
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

    const GridCellRange cellRange = Geometry2d::gridCellRange(rect, m_cellSize);

    for (std::int32_t cellY = cellRange.minY; cellY <= cellRange.maxY; ++cellY) {
        for (std::int32_t cellX = cellRange.minX; cellX <= cellRange.maxX; ++cellX) {
            m_rectIndicesByCell[Geometry2d::packedGridCellKey(cellX, cellY)].push_back(rectIndex);
        }
    }
}

void RectOccupancyGrid::clear()
{
    m_rects.clear();
    m_rectIndicesByCell.clear();
}

}  // namespace skygate::core
