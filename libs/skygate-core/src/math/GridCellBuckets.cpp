#include "GridCellBuckets.hpp"
#include "Geometry2d.hpp"
#include "GridCellRange.hpp"

namespace skygate::core {

void GridCellBuckets::insert(
    CellIndices& cellIndices, const Rect2d& bounds, const double cellSize, const std::size_t index
)
{
    const GridCellRange cellRange = Geometry2d::gridCellRange(bounds, cellSize);

    for (std::int32_t cellY = cellRange.minY; cellY <= cellRange.maxY; ++cellY) {
        for (std::int32_t cellX = cellRange.minX; cellX <= cellRange.maxX; ++cellX) {
            cellIndices[Geometry2d::packedGridCellKey(cellX, cellY)].push_back(index);
        }
    }
}

const std::vector<std::size_t>* GridCellBuckets::bucketAtPoint(
    const CellIndices& cellIndices, const double x, const double y, const double cellSize
) noexcept
{
    return bucketAtCell(cellIndices, Geometry2d::gridCellIndex(x, cellSize), Geometry2d::gridCellIndex(y, cellSize));
}

const std::vector<std::size_t>* GridCellBuckets::bucketAtCell(
    const CellIndices& cellIndices, const std::int32_t cellX, const std::int32_t cellY
) noexcept
{
    const auto cellIt = cellIndices.find(Geometry2d::packedGridCellKey(cellX, cellY));
    return cellIt == cellIndices.end() ? nullptr : &cellIt->second;
}

void GridCellBuckets::clear(CellIndices& cellIndices) noexcept
{
    cellIndices.clear();
}

}  // namespace skygate::core
