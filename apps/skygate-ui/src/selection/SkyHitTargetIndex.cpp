#include "SkyHitTargetIndex.hpp"

#include <algorithm>

namespace {

constexpr double kPointHitTargetRadiusMinPx = 10.0;
constexpr double kPointHitTargetRadiusPaddingPx = 5.0;
constexpr double kGlyphHitTargetRadiusMinPx = 12.0;

}  // namespace

void SkyHitTargetIndex::rebuild(const SkyRenderFrame& frame, const skygate::ephemeris::EphemerisSnapshot& snapshot)
{
    m_targets.clear();
    m_targets.reserve(frame.points.size() + frame.glyphs.size());

    for (const auto& point : frame.points) {
        const auto& body = snapshot.bodyAt(point.bodyIndex);
        if (body.displayName.empty()) {
            continue;
        }

        m_targets.push_back(
            skygate::core::CircleHitTarget{
                .x = point.x,
                .y = point.y,
                .radius = std::max(kPointHitTargetRadiusMinPx, point.sizePx + kPointHitTargetRadiusPaddingPx),
                .payloadId = point.bodyIndex
            }
        );
    }

    for (const auto& glyph : frame.glyphs) {
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (body.displayName.empty()) {
            continue;
        }

        m_targets.push_back(
            skygate::core::CircleHitTarget{
                .x = glyph.x,
                .y = glyph.y,
                .radius = std::max({glyph.radiusXPx, glyph.radiusYPx, kGlyphHitTargetRadiusMinPx}),
                .payloadId = glyph.bodyIndex
            }
        );
    }

    m_hitIndex.rebuild(m_targets);
}

void SkyHitTargetIndex::clear()
{
    m_targets.clear();
    m_hitIndex.clear();
}

std::optional<std::uint32_t> SkyHitTargetIndex::bodyIndexAt(
    const double x,
    const double y,
    const double viewportWidth,
    const double viewportHeight,
    const skygate::ephemeris::EphemerisSnapshot& snapshot
) const
{
    if (viewportWidth <= 0.0 || viewportHeight <= 0.0 || m_targets.empty() || snapshot.catalogBodies == nullptr) {
        return std::nullopt;
    }

    const auto bodyIndex = m_hitIndex.nearestPayloadAt(x, y);
    if (!bodyIndex.has_value() || snapshot.bodyAt(*bodyIndex).displayName.empty()) {
        return std::nullopt;
    }

    return bodyIndex;
}
