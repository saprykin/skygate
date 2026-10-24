#include "CircleHitIndex.hpp"
#include "Geometry2d.hpp"
#include "GridCellBuckets.hpp"
#include "Rect2d.hpp"

#include <cmath>
#include <limits>

namespace skygate::core {

CircleHitIndex::CircleHitIndex(const double cellSize) : m_cellSize(cellSize) {}

void CircleHitIndex::rebuild(const std::span<const CircleHitTarget> targets)
{
    clear();
    m_targets.reserve(targets.size());
    m_targetIndicesByCell.reserve((targets.size() / 4U) + 1U);

    for (const auto& target : targets) {
        if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.radius)
            || target.radius <= 0.0) {
            continue;
        }

        const std::size_t targetIndex = m_targets.size();
        m_targets.push_back(target);

        const Rect2d bounds{
            .left = target.x - target.radius,
            .top = target.y - target.radius,
            .right = target.x + target.radius,
            .bottom = target.y + target.radius,
        };
        GridCellBuckets::insert(m_targetIndicesByCell, bounds, m_cellSize, targetIndex);
    }
}

void CircleHitIndex::clear()
{
    m_targets.clear();
    GridCellBuckets::clear(m_targetIndicesByCell);
}

std::optional<std::uint32_t> CircleHitIndex::nearestPayloadAt(const double x, const double y) const
{
    if (m_targets.empty()) {
        return std::nullopt;
    }

    const std::vector<std::size_t>* targetIndices =
        GridCellBuckets::bucketAtPoint(m_targetIndicesByCell, x, y, m_cellSize);
    if (targetIndices == nullptr) {
        return std::nullopt;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestTargetIndex = m_targets.size();
    for (const std::size_t targetIndex : *targetIndices) {
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
