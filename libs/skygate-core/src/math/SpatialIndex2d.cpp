#include "skygate/core/math/SpatialIndex2d.hpp"

#include <cmath>
#include <limits>

namespace skygate::core {

RectOccupancyGrid::RectOccupancyGrid(const double cellSize)
    : m_cellSize(cellSize)
{
}

bool RectOccupancyGrid::collides(const Rect2d& rect) const
{
    const auto minCellX = gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
            const auto cellIt = m_rectIndicesByCell.find(packedGridCellKey(cellX, cellY));
            if (cellIt == m_rectIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t rectIndex : cellIt->second) {
                if (intersects(rect, m_rects.at(rectIndex))) {
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

    const auto minCellX = gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
            m_rectIndicesByCell[packedGridCellKey(cellX, cellY)].push_back(rectIndex);
        }
    }
}

void RectOccupancyGrid::clear()
{
    m_rects.clear();
    m_rectIndicesByCell.clear();
}

CircleHitIndex::CircleHitIndex(const double cellSize)
    : m_cellSize(cellSize)
{
}

void CircleHitIndex::rebuild(const std::span<const CircleHitTarget> targets)
{
    m_targets.clear();
    m_targetIndicesByCell.clear();
    m_targets.reserve(targets.size());
    m_targetIndicesByCell.reserve((targets.size() / 4U) + 1U);

    for (const auto& target : targets) {
        if (
            !std::isfinite(target.x)
            || !std::isfinite(target.y)
            || !std::isfinite(target.radius)
            || target.radius <= 0.0
        ) {
            continue;
        }

        const std::size_t targetIndex = m_targets.size();
        m_targets.push_back(target);

        const auto minCellX = gridCellIndex(target.x - target.radius, m_cellSize);
        const auto maxCellX = gridCellIndex(target.x + target.radius, m_cellSize);
        const auto minCellY = gridCellIndex(target.y - target.radius, m_cellSize);
        const auto maxCellY = gridCellIndex(target.y + target.radius, m_cellSize);

        for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
            for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
                m_targetIndicesByCell[packedGridCellKey(cellX, cellY)].push_back(targetIndex);
            }
        }
    }
}

void CircleHitIndex::clear()
{
    m_targets.clear();
    m_targetIndicesByCell.clear();
}

std::optional<std::uint32_t> CircleHitIndex::nearestPayloadAt(
    const double x,
    const double y
) const
{
    if (m_targets.empty()) {
        return std::nullopt;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestTargetIndex = m_targets.size();
    const std::int32_t cellX = gridCellIndex(x, m_cellSize);
    const std::int32_t cellY = gridCellIndex(y, m_cellSize);

    const auto cellIt = m_targetIndicesByCell.find(packedGridCellKey(cellX, cellY));
    if (cellIt == m_targetIndicesByCell.end()) {
        return std::nullopt;
    }

    for (const std::size_t targetIndex : cellIt->second) {
        const auto& target = m_targets[targetIndex];
        const double distanceSquared = squaredDistance2d(x, y, target.x, target.y);
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
