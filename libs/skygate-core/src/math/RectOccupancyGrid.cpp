#include "RectOccupancyGrid.hpp"
#include "Geometry2d.hpp"

namespace skygate::core {

RectOccupancyGrid::RectOccupancyGrid(const double cellSize) : m_cellSize(cellSize) {}

bool RectOccupancyGrid::collides(const Rect2d& rect) const
{
    const auto minCellX = Geometry2d::gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = Geometry2d::gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = Geometry2d::gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = Geometry2d::gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
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

    const auto minCellX = Geometry2d::gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = Geometry2d::gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = Geometry2d::gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = Geometry2d::gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
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
