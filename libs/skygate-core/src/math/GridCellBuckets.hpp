#pragma once

#include "Rect2d.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace skygate::core {

// Cell buckets shared by the screen-space grids. Each packed cell key maps to
// the indices of the items that overlap the cell.
class GridCellBuckets final {
public:
    using CellIndices = std::unordered_map<std::uint64_t, std::vector<std::size_t>>;

    GridCellBuckets() = delete;

    // Appends `index` to every cell bucket that `bounds` covers.
    static void insert(CellIndices& cellIndices, const Rect2d& bounds, double cellSize, std::size_t index);

    // Returns the bucket of the cell that contains the point, or nullptr when
    // nothing overlaps that cell.
    [[nodiscard]] static const std::vector<std::size_t>*
    bucketAtPoint(const CellIndices& cellIndices, double x, double y, double cellSize) noexcept;

    [[nodiscard]] static const std::vector<std::size_t>*
    bucketAtCell(const CellIndices& cellIndices, std::int32_t cellX, std::int32_t cellY) noexcept;

    static void clear(CellIndices& cellIndices) noexcept;
};

}  // namespace skygate::core
