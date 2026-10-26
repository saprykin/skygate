#include "SkyRenderBodyBuilder.hpp"
#include "SkyRenderGlyph.hpp"
#include "SkyRenderPoint.hpp"
#include "SkyRenderStylePolicy.hpp"
#include "math/Geometry2d.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kStarDecimationMinStateCount = 30000U;
constexpr double kStarDecimationMinFovDeg = 35.0;
// Field width above the gate at which the cell size reaches its full range.
constexpr double kStarDecimationCellSizeFovSpanDeg = 95.0;
constexpr double kStarDecimationCellSizeBasePx = 2.5;
constexpr double kStarDecimationCellSizeRangePx = 4.5;
// Cell sizes scale with the viewport relative to a reference area tuned here,
// independently of the default window size.
constexpr double kStarDecimationScaleReferenceWidthPx = 1100.0;
constexpr double kStarDecimationScaleReferenceHeightPx = 760.0;
constexpr double kStarDecimationViewportScaleMin = 0.85;
constexpr double kStarDecimationViewportScaleMax = 1.35;
// Extra margin so a glyph that is only partly outside the viewport still
// reaches the render frame.
constexpr double kDeepSkyGlyphMarginPaddingPx = 2.0;

struct DecimatedStarPoint final {
    SkyRenderPoint point;
    double visualMagnitude = 0.0;
    double distanceToCellCenterSquared = 0.0;
};

[[nodiscard]] double starDecimationCellSizePx(
    const skygate::core::ProjectionParams& projectionParams,
    const std::size_t stateCount,
    const double viewportWidth,
    const double viewportHeight
)
{
    if (stateCount < kStarDecimationMinStateCount || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return 0.0;
    }

    if (projectionParams.fovDeg <= kStarDecimationMinFovDeg) {
        return 0.0;
    }

    const double normalizedFov =
        std::clamp((projectionParams.fovDeg - kStarDecimationMinFovDeg) / kStarDecimationCellSizeFovSpanDeg, 0.0, 1.0);
    const double baseCellSize = kStarDecimationCellSizeBasePx + (normalizedFov * kStarDecimationCellSizeRangePx);
    const double viewportScale = std::clamp(
        skygate::core::Geometry2d::areaScale(
            viewportWidth, viewportHeight, kStarDecimationScaleReferenceWidthPx, kStarDecimationScaleReferenceHeightPx
        ),
        kStarDecimationViewportScaleMin,
        kStarDecimationViewportScaleMax
    );
    return baseCellSize * viewportScale;
}

[[nodiscard]] std::uint64_t screenCellKey(const double x, const double y, const double cellSizePx)
{
    return skygate::core::Geometry2d::packedGridCellKey(x, y, cellSizePx);
}

[[nodiscard]] double distanceToCellCenterSquared(const double x, const double y, const double cellSizePx)
{
    const skygate::core::Vector2d center = skygate::core::Geometry2d::gridCellCenter(x, y, cellSizePx);
    return skygate::core::Geometry2d::squaredDistance2d(x, y, center.x, center.y);
}

[[nodiscard]] bool shouldReplaceStarPoint(
    const DecimatedStarPoint& existingPoint, const double visualMagnitude, const double nextDistanceToCellCenterSquared
)
{
    if (visualMagnitude < existingPoint.visualMagnitude) {
        return true;
    }

    if (std::abs(visualMagnitude - existingPoint.visualMagnitude) <= 1e-6) {
        return nextDistanceToCellCenterSquared < existingPoint.distanceToCellCenterSquared;
    }

    return false;
}

[[nodiscard]] SkyRenderPoint makeRenderPoint(
    const skygate::ephemeris::BaseCelestialBody& body,
    const std::uint32_t bodyIndex,
    const skygate::core::ScreenPoint& projected,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    SkyRenderPoint point;
    point.x = projected.x;
    point.y = projected.y;
    point.bodyIndex = bodyIndex;
    point.sizePx = skygate::ui::internal::SkyRenderStylePolicy::pointSizeForMagnitude(body.visualMagnitude);
    if (body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Constellation) {
        point.sizePx = std::max(point.sizePx, 3.0);
    }
    point.color = skygate::ui::internal::SkyRenderStylePolicy::colorForBodyType(body.kind, renderTheme);
    return point;
}

[[nodiscard]] double deepSkyMagnitudeCutoff(const double fovDeg, const double magnitudeCutoff)
{
    if (fovDeg > 90.0) {
        return std::min(magnitudeCutoff, 5.8);
    }
    if (fovDeg > 60.0) {
        return std::min(magnitudeCutoff + 1.0, 7.2);
    }
    if (fovDeg > 35.0) {
        return std::min(magnitudeCutoff + 2.0, 9.0);
    }
    return magnitudeCutoff + 4.0;
}

[[nodiscard]] bool shouldRenderDeepSkyObject(
    const skygate::ephemeris::BaseCelestialBody& body,
    const skygate::core::ProjectionParams& projectionParams,
    const double magnitudeCutoff,
    const SkyOverlayLayerVisibility& overlayLayers
)
{
    if (!overlayLayers.deepSkyObjects) {
        return false;
    }

    if (!std::isfinite(body.visualMagnitude)) {
        return projectionParams.fovDeg <= 35.0;
    }

    return body.visualMagnitude <= deepSkyMagnitudeCutoff(projectionParams.fovDeg, magnitudeCutoff);
}

[[nodiscard]] SkyRenderGlyph makeRenderGlyph(
    const skygate::ephemeris::BaseCelestialBody& body,
    const std::uint32_t bodyIndex,
    const skygate::core::ScreenPoint& projected,
    const skygate::core::ProjectionParams& projectionParams,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    const skygate::ephemeris::DeepSkyObjectInfo defaultInfo;
    const auto& info = body.deepSkyObjectValue().has_value() ? *body.deepSkyObjectValue() : defaultInfo;
    const skygate::ui::internal::SkyRenderStylePolicy::GlyphRadii radii =
        skygate::ui::internal::SkyRenderStylePolicy::deepSkyGlyphRadiiPx(body, projectionParams);

    SkyRenderGlyph glyph;
    glyph.x = projected.x;
    glyph.y = projected.y;
    glyph.bodyIndex = bodyIndex;
    glyph.kind = info.kind;
    glyph.radiusXPx = radii.radiusXPx;
    glyph.radiusYPx = radii.radiusYPx;
    glyph.rotationDeg = info.positionAngleDeg.value_or(0.0);
    glyph.widthPx = 1.25;
    glyph.color = skygate::ui::internal::SkyRenderStylePolicy::colorForBodyType(body.kind, renderTheme);
    return glyph;
}

[[nodiscard]] double deepSkyGlyphMarginPx(
    const skygate::ephemeris::BaseCelestialBody& body, const skygate::core::ProjectionParams& projectionParams
)
{
    const skygate::ui::internal::SkyRenderStylePolicy::GlyphRadii radii =
        skygate::ui::internal::SkyRenderStylePolicy::deepSkyGlyphRadiiPx(body, projectionParams);
    return std::max(radii.radiusXPx, radii.radiusYPx) + kDeepSkyGlyphMarginPaddingPx;
}

}  // namespace

namespace skygate::ui::internal {

void SkyRenderBodyBuilder::appendBodies(
    SkyRenderFrame& frame,
    SkyRenderHorizontalLookup* horizontalLookup,
    const skygate::ephemeris::EphemerisSnapshot& snapshot,
    const skygate::core::PreparedProjection& projection,
    const double magnitudeCutoff,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers
) const
{
    const double starCellSizePx =
        starDecimationCellSizePx(projection.params(), snapshot.states.size(), viewportWidth, viewportHeight);
    std::unordered_map<std::uint64_t, std::size_t> starPointIndexByCell;
    std::vector<DecimatedStarPoint> decimatedStarPoints;
    frame.points.reserve(snapshot.states.size() / 8U);
    frame.glyphs.reserve(128U);
    if (starCellSizePx > 0.0) {
        starPointIndexByCell.reserve(snapshot.states.size() / 6U);
        decimatedStarPoints.reserve(snapshot.states.size() / 6U);
    }

    for (const auto& state : snapshot.states) {
        if (!state.horizontal.isFinite()) {
            continue;
        }

        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (horizontalLookup != nullptr) {
            horizontalLookup->capture(body, state.horizontal);
        }

        if (body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Star && body.visualMagnitude > magnitudeCutoff) {
            continue;
        }

        if (body.kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject
            && !shouldRenderDeepSkyObject(body, projection.params(), magnitudeCutoff, overlayLayers)) {
            continue;
        }

        const bool isDeepSkyObject = body.kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
        const auto projected =
            isDeepSkyObject
                ? projection.projectWithMargin(state.horizontal, deepSkyGlyphMarginPx(body, projection.params()))
                : projection.project(state.horizontal);
        if (!projected.isVisible() || !projected.isFinite()) {
            continue;
        }

        if (isDeepSkyObject) {
            frame.glyphs.push_back(makeRenderGlyph(body, state.bodyIndex, projected, projection.params(), renderTheme));
            continue;
        }

        if (body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Star && starCellSizePx > 0.0) {
            const std::uint64_t cellKey = screenCellKey(projected.x, projected.y, starCellSizePx);
            const double cellCenterDistanceSquared =
                distanceToCellCenterSquared(projected.x, projected.y, starCellSizePx);
            if (const auto pointIndexIt = starPointIndexByCell.find(cellKey);
                pointIndexIt != starPointIndexByCell.end()) {
                auto& existingPoint = decimatedStarPoints[pointIndexIt->second];
                if (!shouldReplaceStarPoint(existingPoint, body.visualMagnitude, cellCenterDistanceSquared)) {
                    continue;
                }

                existingPoint.point = makeRenderPoint(body, state.bodyIndex, projected, renderTheme);
                existingPoint.visualMagnitude = body.visualMagnitude;
                existingPoint.distanceToCellCenterSquared = cellCenterDistanceSquared;
                continue;
            }

            starPointIndexByCell.emplace(cellKey, decimatedStarPoints.size());
            decimatedStarPoints.push_back(
                DecimatedStarPoint{
                    .point = makeRenderPoint(body, state.bodyIndex, projected, renderTheme),
                    .visualMagnitude = body.visualMagnitude,
                    .distanceToCellCenterSquared = cellCenterDistanceSquared
                }
            );
            continue;
        }

        frame.points.push_back(makeRenderPoint(body, state.bodyIndex, projected, renderTheme));
    }

    if (!decimatedStarPoints.empty()) {
        frame.points.reserve(frame.points.size() + decimatedStarPoints.size());
        for (auto& starPoint : decimatedStarPoints) {
            frame.points.push_back(std::move(starPoint.point));
        }
    }
}

}  // namespace skygate::ui::internal
