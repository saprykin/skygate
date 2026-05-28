#include "CircleHitIndex.hpp"
#include "Geometry2d.hpp"

#include <cmath>
#include <limits>

namespace skygate::core {

CircleHitIndex::CircleHitIndex(const double cellSize) : m_cellSize(cellSize) {}

void CircleHitIndex::rebuild(const std::span<const CircleHitTarget> targets)
{
    m_targets.clear();
    m_targetIndicesByCell.clear();
    m_targets.reserve(targets.size());
    m_targetIndicesByCell.reserve((targets.size() / 4U) + 1U);

    for (const auto& target : targets) {
        if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.radius)
            || target.radius <= 0.0) {
            continue;
        }

        const std::size_t targetIndex = m_targets.size();
        m_targets.push_back(target);

        const auto minCellX = Geometry2d::gridCellIndex(target.x - target.radius, m_cellSize);
        const auto maxCellX = Geometry2d::gridCellIndex(target.x + target.radius, m_cellSize);
        const auto minCellY = Geometry2d::gridCellIndex(target.y - target.radius, m_cellSize);
        const auto maxCellY = Geometry2d::gridCellIndex(target.y + target.radius, m_cellSize);

        for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
            for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
                m_targetIndicesByCell[Geometry2d::packedGridCellKey(cellX, cellY)].push_back(targetIndex);
            }
        }
    }
}

void CircleHitIndex::clear()
{
    m_targets.clear();
    m_targetIndicesByCell.clear();
}

std::optional<std::uint32_t> CircleHitIndex::nearestPayloadAt(const double x, const double y) const
{
    if (m_targets.empty()) {
        return std::nullopt;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestTargetIndex = m_targets.size();
    const std::int32_t cellX = Geometry2d::gridCellIndex(x, m_cellSize);
    const std::int32_t cellY = Geometry2d::gridCellIndex(y, m_cellSize);

    const auto cellIt = m_targetIndicesByCell.find(Geometry2d::packedGridCellKey(cellX, cellY));
    if (cellIt == m_targetIndicesByCell.end()) {
        return std::nullopt;
    }

    for (const std::size_t targetIndex : cellIt->second) {
        const auto& target = m_targets[targetIndex];
        const double distanceSquared = Geometry2d::squaredDistance2d(x, y, target.x, target.y);
        if (distanceSquared > (target.radius * target.radius)) {
            continue;
        }

        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestTargetIndex = targetIndex;
        }
    }

    if (bestTargetIndex >= m_targets.size()) {
        return std::nullopt;
    }

    return m_targets[bestTargetIndex].payloadId;
}

}  // namespace skygate::core
