#include "skygate/core/math/ScreenGeometry.hpp"

#include "skygate/core/math/GeometryMath.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace skygate::core {

bool intersects(const Rect2d& lhs, const Rect2d& rhs) noexcept
{
    return lhs.left < rhs.right
        && lhs.right > rhs.left
        && lhs.top < rhs.bottom
        && lhs.bottom > rhs.top;
}

bool fitsWithin(
    const Rect2d& rect,
    const double width,
    const double height,
    const double margin
) noexcept
{
    return rect.left >= margin
        && rect.right <= (width - margin)
        && rect.top >= margin
        && rect.bottom <= (height - margin);
}

RectOccupancyGrid::RectOccupancyGrid(const double cellSize)
    : m_cellSize(cellSize)
{
}

bool RectOccupancyGrid::collides(const Rect2d& rect) const
{
    const auto minCellX = GeometryMath::gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = GeometryMath::gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = GeometryMath::gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = GeometryMath::gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
            const auto cellIt = m_rectIndicesByCell.find(
                GeometryMath::packedGridCellKey(cellX, cellY)
            );
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

    const auto minCellX = GeometryMath::gridCellIndex(rect.left, m_cellSize);
    const auto maxCellX = GeometryMath::gridCellIndex(rect.right, m_cellSize);
    const auto minCellY = GeometryMath::gridCellIndex(rect.top, m_cellSize);
    const auto maxCellY = GeometryMath::gridCellIndex(rect.bottom, m_cellSize);

    for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
            m_rectIndicesByCell[GeometryMath::packedGridCellKey(cellX, cellY)].push_back(
                rectIndex
            );
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

        const auto minCellX = GeometryMath::gridCellIndex(target.x - target.radius, m_cellSize);
        const auto maxCellX = GeometryMath::gridCellIndex(target.x + target.radius, m_cellSize);
        const auto minCellY = GeometryMath::gridCellIndex(target.y - target.radius, m_cellSize);
        const auto maxCellY = GeometryMath::gridCellIndex(target.y + target.radius, m_cellSize);

        for (std::int32_t cellY = minCellY; cellY <= maxCellY; ++cellY) {
            for (std::int32_t cellX = minCellX; cellX <= maxCellX; ++cellX) {
                m_targetIndicesByCell[GeometryMath::packedGridCellKey(cellX, cellY)].push_back(
                    targetIndex
                );
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
    const std::int32_t cellX = GeometryMath::gridCellIndex(x, m_cellSize);
    const std::int32_t cellY = GeometryMath::gridCellIndex(y, m_cellSize);

    const auto cellIt = m_targetIndicesByCell.find(
        GeometryMath::packedGridCellKey(cellX, cellY)
    );
    if (cellIt == m_targetIndicesByCell.end()) {
        return std::nullopt;
    }

    for (const std::size_t targetIndex : cellIt->second) {
        const auto& target = m_targets[targetIndex];
        const double distanceSquared = GeometryMath::squaredDistance2d(
            x,
            y,
            target.x,
            target.y
        );
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

std::vector<LineSegment2d> ProjectedPolylineBuilder::build(
    const PreparedProjection& projection,
    const std::span<const HorizontalCoordinate> coordinates,
    const double maxSegmentLengthSquared
) const
{
    std::vector<LineSegment2d> segments;
    if (coordinates.size() < 2U) {
        return segments;
    }

    bool hasPreviousPoint = false;
    ScreenPoint previousPoint;
    for (const HorizontalCoordinate& coordinate : coordinates) {
        const auto projected = projection.project(coordinate);
        if (!projected.isVisible || !projected.isFinite()) {
            hasPreviousPoint = false;
            continue;
        }

        if (hasPreviousPoint) {
            const double segmentLengthSquared = GeometryMath::squaredDistance2d(
                projected.x,
                projected.y,
                previousPoint.x,
                previousPoint.y
            );
            if (segmentLengthSquared <= maxSegmentLengthSquared) {
                segments.push_back(LineSegment2d {
                    .x1 = previousPoint.x,
                    .y1 = previousPoint.y,
                    .x2 = projected.x,
                    .y2 = projected.y
                });
            }
        }

        previousPoint = projected;
        hasPreviousPoint = true;
    }

    return segments;
}

std::vector<LineSegment2d> DashedLineBuilder::build(
    const LineSegment2d& segment,
    const double dashLength,
    const double gapLength
) const
{
    std::vector<LineSegment2d> segments;
    const double deltaX = segment.x2 - segment.x1;
    const double deltaY = segment.y2 - segment.y1;
    const double length = std::hypot(deltaX, deltaY);
    if (
        length <= 1e-9
        || !std::isfinite(length)
        || dashLength <= 0.0
        || gapLength < 0.0
    ) {
        return segments;
    }

    const double unitX = deltaX / length;
    const double unitY = deltaY / length;
    double dashStart = 0.0;
    while (dashStart < length) {
        const double dashEnd = std::min(dashStart + dashLength, length);
        segments.push_back(LineSegment2d {
            .x1 = segment.x1 + (unitX * dashStart),
            .y1 = segment.y1 + (unitY * dashStart),
            .x2 = segment.x1 + (unitX * dashEnd),
            .y2 = segment.y1 + (unitY * dashEnd)
        });
        dashStart += dashLength + gapLength;
    }

    return segments;
}

}  // namespace skygate::core
