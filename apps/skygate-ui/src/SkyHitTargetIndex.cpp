#include "SkyHitTargetIndex.hpp"

#include "skygate/core/math/GeometryMath.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kHoverLookupCellSizePx = 24.0;

[[nodiscard]] std::uint64_t hoverCellKey(const double x, const double y) noexcept
{
    return skygate::core::GeometryMath::packedGridCellKey(x, y, kHoverLookupCellSizePx);
}

}  // namespace

void SkyHitTargetIndex::rebuild(
    const SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot
)
{
    m_targets.clear();
    m_targets.reserve(frame.points.size() + frame.glyphs.size());
    m_targetIndicesByCell.clear();
    m_targetIndicesByCell.reserve(((frame.points.size() + frame.glyphs.size()) / 4U) + 1U);

    for (const auto& point : frame.points) {
        const auto& body = snapshot.bodyAt(point.bodyIndex);
        if (body.displayName.empty()) {
            continue;
        }

        const std::size_t targetIndex = m_targets.size();
        m_targets.push_back(Target {
            .bodyIndex = point.bodyIndex,
            .x = point.x,
            .y = point.y,
            .radiusPx = std::max(10.0, point.sizePx + 5.0)
        });
        m_targetIndicesByCell[hoverCellKey(point.x, point.y)].push_back(targetIndex);
    }

    for (const auto& glyph : frame.glyphs) {
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (body.displayName.empty()) {
            continue;
        }

        const std::size_t targetIndex = m_targets.size();
        m_targets.push_back(Target {
            .bodyIndex = glyph.bodyIndex,
            .x = glyph.x,
            .y = glyph.y,
            .radiusPx = std::max({glyph.radiusXPx, glyph.radiusYPx, 12.0})
        });
        m_targetIndicesByCell[hoverCellKey(glyph.x, glyph.y)].push_back(targetIndex);
    }
}

void SkyHitTargetIndex::clear()
{
    m_targets.clear();
    m_targetIndicesByCell.clear();
}

std::optional<std::uint32_t> SkyHitTargetIndex::bodyIndexAt(
    const double x,
    const double y,
    const double viewportWidth,
    const double viewportHeight,
    const skygate::ephemeris::SkySnapshot& snapshot
) const
{
    if (
        viewportWidth <= 0.0
        || viewportHeight <= 0.0
        || m_targets.empty()
        || snapshot.catalogBodies == nullptr
    ) {
        return std::nullopt;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestTargetIndex = m_targets.size();
    const std::int32_t cellX =
        skygate::core::GeometryMath::gridCellIndex(x, kHoverLookupCellSizePx);
    const std::int32_t cellY =
        skygate::core::GeometryMath::gridCellIndex(y, kHoverLookupCellSizePx);

    for (int deltaY = -1; deltaY <= 1; ++deltaY) {
        for (int deltaX = -1; deltaX <= 1; ++deltaX) {
            const auto cellIt = m_targetIndicesByCell.find(
                skygate::core::GeometryMath::packedGridCellKey(
                    cellX + deltaX,
                    cellY + deltaY
                )
            );
            if (cellIt == m_targetIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t targetIndex : cellIt->second) {
                const auto& target = m_targets[targetIndex];
                const auto& body = snapshot.bodyAt(target.bodyIndex);
                if (body.displayName.empty()) {
                    continue;
                }

                const double distanceSquared = skygate::core::GeometryMath::squaredDistance2d(
                    x,
                    y,
                    target.x,
                    target.y
                );
                if (distanceSquared > (target.radiusPx * target.radiusPx)) {
                    continue;
                }

                if (distanceSquared < bestDistanceSquared) {
                    bestDistanceSquared = distanceSquared;
                    bestTargetIndex = targetIndex;
                }
            }
        }
    }

    if (bestTargetIndex >= m_targets.size()) {
        return std::nullopt;
    }

    return m_targets[bestTargetIndex].bodyIndex;
}
