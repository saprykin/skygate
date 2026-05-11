#include "SkyRenderBodyBuilder.hpp"

#include "SkyContextControllerSupport.hpp"

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/SpatialIndex2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

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
    if (stateCount < 30000U || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return 0.0;
    }

    if (projectionParams.fovDeg <= 35.0) {
        return 0.0;
    }

    const double normalizedFov = std::clamp((projectionParams.fovDeg - 35.0) / 95.0, 0.0, 1.0);
    const double baseCellSize = 2.5 + (normalizedFov * 4.5);
    const double viewportScale = std::clamp(
        skygate::core::areaScale(viewportWidth, viewportHeight, 1100.0, 760.0),
        0.85,
        1.35
    );
    return baseCellSize * viewportScale;
}

[[nodiscard]] std::uint64_t screenCellKey(
    const double x,
    const double y,
    const double cellSizePx
)
{
    return skygate::core::packedGridCellKey(x, y, cellSizePx);
}

[[nodiscard]] double distanceToCellCenterSquared(
    const double x,
    const double y,
    const double cellSizePx
)
{
    const skygate::core::Vector2d center =
        skygate::core::gridCellCenter(x, y, cellSizePx);
    return skygate::core::squaredDistance2d(x, y, center.x, center.y);
}

[[nodiscard]] bool shouldReplaceStarPoint(
    const DecimatedStarPoint& existingPoint,
    const double visualMagnitude,
    const double nextDistanceToCellCenterSquared
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
    const skygate::ephemeris::CelestialBody& body,
    const std::uint32_t bodyIndex,
    const skygate::core::ScreenPoint& projected,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    SkyRenderPoint point;
    point.x = projected.x;
    point.y = projected.y;
    point.bodyIndex = bodyIndex;
    point.sizePx = skygate::ui::internal::SkyContextRenderStyle::pointSizeForMagnitude(
        body.visualMagnitude
    );
    if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
        point.sizePx = std::max(point.sizePx, 3.0);
    }
    point.color = skygate::ui::internal::SkyContextRenderStyle::colorForBodyType(
        body.type,
        renderTheme
    );
    return point;
}

[[nodiscard]] double deepSkyMagnitudeCutoff(
    const double fovDeg,
    const double magnitudeCutoff
)
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
    const skygate::ephemeris::CelestialBody& body,
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

    return body.visualMagnitude <= deepSkyMagnitudeCutoff(
        projectionParams.fovDeg,
        magnitudeCutoff
    );
}

[[nodiscard]] SkyRenderGlyph makeRenderGlyph(
    const skygate::ephemeris::CelestialBody& body,
    const std::uint32_t bodyIndex,
    const skygate::core::ScreenPoint& projected,
    const skygate::core::ProjectionParams& projectionParams,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    const skygate::ephemeris::DeepSkyObjectInfo defaultInfo;
    const auto& info = body.deepSkyObject.has_value() ? *body.deepSkyObject : defaultInfo;
    const double pixelsPerDeg = std::min(
        projectionParams.viewportWidth,
        projectionParams.viewportHeight
    ) / std::max(1.0, projectionParams.fovDeg);
    const double majorArcmin = info.majorAxisArcmin.value_or(10.0);
    const double minorArcmin = info.minorAxisArcmin.value_or(majorArcmin);
    const double radiusX = ((majorArcmin / 60.0) * pixelsPerDeg) * 0.5;
    const double radiusY = ((minorArcmin / 60.0) * pixelsPerDeg) * 0.5;

    SkyRenderGlyph glyph;
    glyph.x = projected.x;
    glyph.y = projected.y;
    glyph.bodyIndex = bodyIndex;
    glyph.kind = info.kind;
    glyph.radiusXPx = std::clamp(radiusX, 4.5, 44.0);
    glyph.radiusYPx = std::clamp(radiusY, 4.0, 32.0);
    if (info.kind == skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula) {
        glyph.radiusXPx = std::clamp(radiusX, 5.0, 12.0);
        glyph.radiusYPx = glyph.radiusXPx;
    }
    glyph.rotationDeg = info.positionAngleDeg.value_or(0.0);
    glyph.widthPx = 1.25;
    glyph.color = skygate::ui::internal::SkyContextRenderStyle::colorForBodyType(
        body.type,
        renderTheme
    );
    return glyph;
}

[[nodiscard]] double deepSkyGlyphMarginPx(
    const skygate::ephemeris::CelestialBody& body,
    const skygate::core::ProjectionParams& projectionParams
)
{
    const skygate::ephemeris::DeepSkyObjectInfo defaultInfo;
    const auto& info = body.deepSkyObject.has_value() ? *body.deepSkyObject : defaultInfo;
    const double pixelsPerDeg = std::min(
        projectionParams.viewportWidth,
        projectionParams.viewportHeight
    ) / std::max(1.0, projectionParams.fovDeg);
    const double majorArcmin = info.majorAxisArcmin.value_or(10.0);
    const double minorArcmin = info.minorAxisArcmin.value_or(majorArcmin);
    double radiusX = std::clamp(((majorArcmin / 60.0) * pixelsPerDeg) * 0.5, 4.5, 44.0);
    double radiusY = std::clamp(((minorArcmin / 60.0) * pixelsPerDeg) * 0.5, 4.0, 32.0);
    if (info.kind == skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula) {
        radiusX = std::clamp(((majorArcmin / 60.0) * pixelsPerDeg) * 0.5, 5.0, 12.0);
        radiusY = radiusX;
    }

    return std::max(radiusX, radiusY) + 2.0;
}

}  // namespace

namespace skygate::ui::internal {

void SkyRenderBodyBuilder::appendBodies(
    SkyRenderFrame& frame,
    SkyRenderHorizontalLookup* horizontalLookup,
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::PreparedProjection& projection,
    const double magnitudeCutoff,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers
) const
{
    const double starCellSizePx = starDecimationCellSizePx(
        projection.params(),
        snapshot.states.size(),
        viewportWidth,
        viewportHeight
    );
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

        if (
            body.type == skygate::ephemeris::CelestialBodyType::Star
            && body.visualMagnitude > magnitudeCutoff
        ) {
            continue;
        }

        if (
            body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject
            && !shouldRenderDeepSkyObject(
                body,
                projection.params(),
                magnitudeCutoff,
                overlayLayers
            )
        ) {
            continue;
        }

        const bool isDeepSkyObject =
            body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject;
        const auto projected = isDeepSkyObject
            ? projection.projectWithMargin(
                state.horizontal,
                deepSkyGlyphMarginPx(body, projection.params())
            )
            : projection.project(state.horizontal);
        if (!projected.isVisible || !projected.isFinite()) {
            continue;
        }

        if (isDeepSkyObject) {
            frame.glyphs.push_back(makeRenderGlyph(
                body,
                state.bodyIndex,
                projected,
                projection.params(),
                renderTheme
            ));
            continue;
        }

        if (
            body.type == skygate::ephemeris::CelestialBodyType::Star
            && starCellSizePx > 0.0
        ) {
            const std::uint64_t cellKey = screenCellKey(projected.x, projected.y, starCellSizePx);
            const double cellCenterDistanceSquared = distanceToCellCenterSquared(
                projected.x,
                projected.y,
                starCellSizePx
            );
            if (const auto pointIndexIt = starPointIndexByCell.find(cellKey);
                pointIndexIt != starPointIndexByCell.end()) {
                auto& existingPoint = decimatedStarPoints[pointIndexIt->second];
                if (!shouldReplaceStarPoint(
                        existingPoint,
                        body.visualMagnitude,
                        cellCenterDistanceSquared
                    )) {
                    continue;
                }

                existingPoint.point = makeRenderPoint(
                    body,
                    state.bodyIndex,
                    projected,
                    renderTheme
                );
                existingPoint.visualMagnitude = body.visualMagnitude;
                existingPoint.distanceToCellCenterSquared = cellCenterDistanceSquared;
                continue;
            }

            starPointIndexByCell.emplace(cellKey, decimatedStarPoints.size());
            decimatedStarPoints.push_back(DecimatedStarPoint {
                .point = makeRenderPoint(body, state.bodyIndex, projected, renderTheme),
                .visualMagnitude = body.visualMagnitude,
                .distanceToCellCenterSquared = cellCenterDistanceSquared
            });
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
