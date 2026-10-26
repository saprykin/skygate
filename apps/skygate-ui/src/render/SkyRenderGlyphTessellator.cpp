#include "SkyRenderGlyphTessellator.hpp"
#include "math/Geometry2d.hpp"

#include <array>
#include <utility>

namespace {

constexpr int kGlyphEllipseSampleCount = 32;

[[nodiscard]] std::pair<float, float> rotatedGlyphPoint(const SkyRenderGlyph& glyph, const double x, const double y)
{
    const auto point = skygate::core::Geometry2d::rotatedOffsetPoint2d(glyph.x, glyph.y, x, y, glyph.rotationDeg);
    return {static_cast<float>(point.x), static_cast<float>(point.y)};
}

void appendGlyphCircle(
    skygate::ui::internal::SkyViewportSegmentSink& sink,
    const SkyRenderGlyph& glyph,
    const double radiusXScale,
    const double radiusYScale
)
{
    const auto previousOffset = skygate::core::Geometry2d::ellipseOffsetPoint2d(
        glyph.radiusXPx * radiusXScale, glyph.radiusYPx * radiusYScale, 0, kGlyphEllipseSampleCount
    );
    auto previous = rotatedGlyphPoint(glyph, previousOffset.x, previousOffset.y);
    for (int index = 1; index <= kGlyphEllipseSampleCount; ++index) {
        const auto nextOffset = skygate::core::Geometry2d::ellipseOffsetPoint2d(
            glyph.radiusXPx * radiusXScale, glyph.radiusYPx * radiusYScale, index, kGlyphEllipseSampleCount
        );
        const auto next = rotatedGlyphPoint(glyph, nextOffset.x, nextOffset.y);
        sink.append(
            previous.first, previous.second, next.first, next.second, static_cast<float>(glyph.widthPx), glyph.color
        );
        previous = next;
    }
}

void appendGlyphCross(
    skygate::ui::internal::SkyViewportSegmentSink& sink, const SkyRenderGlyph& glyph, const double scale
)
{
    const auto left = rotatedGlyphPoint(glyph, -glyph.radiusXPx * scale, 0.0);
    const auto right = rotatedGlyphPoint(glyph, glyph.radiusXPx * scale, 0.0);
    const auto top = rotatedGlyphPoint(glyph, 0.0, -glyph.radiusYPx * scale);
    const auto bottom = rotatedGlyphPoint(glyph, 0.0, glyph.radiusYPx * scale);
    sink.append(left.first, left.second, right.first, right.second, static_cast<float>(glyph.widthPx), glyph.color);
    sink.append(top.first, top.second, bottom.first, bottom.second, static_cast<float>(glyph.widthPx), glyph.color);
}

void appendGlyphDiamond(skygate::ui::internal::SkyViewportSegmentSink& sink, const SkyRenderGlyph& glyph)
{
    const std::array<std::pair<float, float>, 4> points{{
        rotatedGlyphPoint(glyph, 0.0, -glyph.radiusYPx),
        rotatedGlyphPoint(glyph, glyph.radiusXPx, 0.0),
        rotatedGlyphPoint(glyph, 0.0, glyph.radiusYPx),
        rotatedGlyphPoint(glyph, -glyph.radiusXPx, 0.0),
    }};
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& start = points[index];
        const auto& end = points[(index + 1U) % points.size()];
        sink.append(start.first, start.second, end.first, end.second, static_cast<float>(glyph.widthPx), glyph.color);
    }
}

void appendDeepSkyGlyph(skygate::ui::internal::SkyViewportSegmentSink& sink, const SkyRenderGlyph& glyph)
{
    switch (glyph.kind) {
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy:
        appendGlyphCircle(sink, glyph, 1.0, 1.0);
        appendGlyphCircle(sink, glyph, 0.58, 0.58);
        break;
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::OpenCluster:
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::Asterism:
        appendGlyphCircle(sink, glyph, 1.0, 1.0);
        break;
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::GlobularCluster:
        appendGlyphCircle(sink, glyph, 1.0, 1.0);
        appendGlyphCross(sink, glyph, 0.62);
        break;
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::Nebula:
        appendGlyphDiamond(sink, glyph);
        appendGlyphCircle(sink, glyph, 0.68, 0.68);
        break;
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::PlanetaryNebula:
        appendGlyphCircle(sink, glyph, 1.0, 1.0);
        appendGlyphCross(sink, glyph, 1.0);
        break;
    case skygate::ephemeris::DeepSkyObjectInfo::Kind::Unknown:
        appendGlyphDiamond(sink, glyph);
        break;
    }
}

}  // namespace

namespace skygate::ui::internal {

void SkyRenderGlyphTessellator::appendGlyphs(
    SkyViewportSegmentSink& sink, const std::span<const SkyRenderGlyph> renderGlyphs
)
{
    for (const auto& glyph : renderGlyphs) {
        appendDeepSkyGlyph(sink, glyph);
    }
}

}  // namespace skygate::ui::internal
