#pragma once

#include "CircleHitTarget.hpp"
#include "GridCellBuckets.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace skygate::core {

class CircleHitIndex final {
public:
    explicit CircleHitIndex(double cellSize);

    void rebuild(std::span<const CircleHitTarget> targets);
    void clear();
    [[nodiscard]] std::optional<std::uint32_t> nearestPayloadAt(double x, double y) const;

private:
    double m_cellSize = 24.0;
    std::vector<CircleHitTarget> m_targets;
    GridCellBuckets::CellIndices m_targetIndicesByCell;
};

}  // namespace skygate::core
