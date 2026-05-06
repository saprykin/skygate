#include "SkyRenderBuilders.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QVariantMap>

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/ProjectedPolylineBuilder.hpp"
#include "skygate/core/math/SpatialIndex2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace skygate::ui::internal;

namespace {
constexpr double kConstellationLineWidthPx = 1.8;

class HorizontalLookup final {
public:
    HorizontalLookup(
        const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs
    )
    {
        for (const auto& lineRef : lineRefs) {
            trackBodyId(lineRef.first);
            trackBodyId(lineRef.second);
        }

        for (const auto& labelRef : labelRefs) {
            for (const std::string& hipId : labelRef.second) {
                trackBodyId(hipId);
            }
        }

        m_horizontalByBodyId.reserve(m_requiredBodyIds.size());
    }

    void capture(
        const skygate::ephemeris::CelestialBody& body,
        const skygate::core::HorizontalCoordinate& horizontal
    )
    {
        if (!m_requiredBodyIds.empty() && m_requiredBodyIds.contains(body.id)) {
            m_horizontalByBodyId.try_emplace(body.id, horizontal);
        }
    }

    [[nodiscard]] const skygate::core::HorizontalCoordinate* findHorizontal(
        const std::string_view bodyId
    ) const
    {
        if (const auto idIt = m_horizontalByBodyId.find(bodyId); idIt != m_horizontalByBodyId.end()) {
            return &idIt->second;
        }

        return nullptr;
    }

private:
    void trackBodyId(const std::string_view bodyId)
    {
        if (bodyId.empty()) {
            return;
        }

        m_requiredBodyIds.emplace(bodyId);
    }

private:
    std::unordered_set<std::string> m_requiredBodyIds;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> m_horizontalByBodyId;
};

struct DecimatedStarPoint final {
    SkyRenderPoint point;
    double visualMagnitude = 0.0;
    double distanceToCellCenterSquared = 0.0;
};

struct DeepSkyLabelCandidate final {
    std::size_t glyphIndex = 0;
    double score = 0.0;
    skygate::core::Rect2d bounds;
};

[[nodiscard]] QColor labelColorForBodyType(
    const skygate::ephemeris::CelestialBodyType type,
    const SkyThemeRenderPalette& renderTheme
)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return renderTheme.labelSun;
    case skygate::ephemeris::CelestialBodyType::Moon:
        return renderTheme.labelMoon;
    case skygate::ephemeris::CelestialBodyType::Planet:
        return renderTheme.labelPlanet;
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return renderTheme.labelConstellation;
    case skygate::ephemeris::CelestialBodyType::DeepSkyObject:
        return renderTheme.labelDeepSkyObject;
    case skygate::ephemeris::CelestialBodyType::Star:
        break;
    }

    return renderTheme.labelDefault;
}

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
    const SkyThemeRenderPalette& renderTheme
)
{
    SkyRenderPoint point;
    point.x = projected.x;
    point.y = projected.y;
    point.bodyIndex = bodyIndex;
    point.sizePx = SkyContextRenderStyle::pointSizeForMagnitude(body.visualMagnitude);
    if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
        point.sizePx = std::max(point.sizePx, 3.0);
    }
    point.color = SkyContextRenderStyle::colorForBodyType(body.type, renderTheme);
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
    const SkyThemeRenderPalette& renderTheme
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
    glyph.color = SkyContextRenderStyle::colorForBodyType(body.type, renderTheme);
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

[[nodiscard]] double deepSkyHitRadius(const SkyRenderGlyph& glyph)
{
    return std::max({glyph.radiusXPx, glyph.radiusYPx, 8.0});
}

[[nodiscard]] bool startsWithCatalogPrefix(
    const std::string_view text,
    const std::string_view prefix
)
{
    return text.size() > prefix.size()
        && std::equal(
            prefix.begin(),
            prefix.end(),
            text.begin(),
            text.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
            [](const char lhs, const char rhs) {
                return std::tolower(
                    static_cast<unsigned char>(lhs)
                ) == std::tolower(static_cast<unsigned char>(rhs));
            }
        )
        && text.at(prefix.size()) == ' ';
}

[[nodiscard]] bool isMessierObject(const skygate::ephemeris::CelestialBody& body)
{
    if (body.id.starts_with("messier_")) {
        return true;
    }

    if (body.displayName.size() < 2U || body.displayName.front() != 'M') {
        return false;
    }

    return std::all_of(
        body.displayName.begin() + 1,
        body.displayName.end(),
        [](const char value) {
            return std::isdigit(static_cast<unsigned char>(value));
        }
    );
}

[[nodiscard]] bool isGenericCatalogAlias(const std::string_view alias)
{
    return startsWithCatalogPrefix(alias, "M")
        || startsWithCatalogPrefix(alias, "NGC")
        || startsWithCatalogPrefix(alias, "IC")
        || startsWithCatalogPrefix(alias, "PGC")
        || startsWithCatalogPrefix(alias, "UGC")
        || startsWithCatalogPrefix(alias, "ESO")
        || startsWithCatalogPrefix(alias, "MCG")
        || startsWithCatalogPrefix(alias, "CGCG")
        || startsWithCatalogPrefix(alias, "2MASX")
        || startsWithCatalogPrefix(alias, "IRAS")
        || startsWithCatalogPrefix(alias, "PK");
}

[[nodiscard]] bool hasCommonNameAlias(const skygate::ephemeris::CelestialBody& body)
{
    if (!body.deepSkyObject.has_value()) {
        return false;
    }

    return std::any_of(
        body.deepSkyObject->aliases.begin(),
        body.deepSkyObject->aliases.end(),
        [](const std::string& alias) {
            return !alias.empty() && !isGenericCatalogAlias(alias);
        }
    );
}

[[nodiscard]] bool shouldConsiderDeepSkyLabel(
    const skygate::ephemeris::CelestialBody& body,
    const double fovDeg
)
{
    if (isMessierObject(body) || hasCommonNameAlias(body)) {
        return true;
    }

    return fovDeg <= 20.0;
}

[[nodiscard]] std::size_t deepSkyLabelBudget(
    const double fovDeg,
    const double viewportWidth,
    const double viewportHeight
)
{
    if (fovDeg > 60.0) {
        return 0U;
    }
    if (fovDeg <= 2.0) {
        return 1000U;
    }

    std::size_t baseBudget = 150U;
    if (fovDeg > 35.0) {
        baseBudget = 18U;
    } else if (fovDeg > 20.0) {
        baseBudget = 32U;
    } else if (fovDeg > 10.0) {
        baseBudget = 60U;
    } else if (fovDeg > 5.0) {
        baseBudget = 100U;
    }

    const double viewportScale = std::clamp(
        skygate::core::areaScale(viewportWidth, viewportHeight, 1100.0, 760.0),
        0.70,
        1.35
    );
    return static_cast<std::size_t>(std::max(
        1.0,
        static_cast<double>(baseBudget) * viewportScale
    ));
}

[[nodiscard]] double deepSkyLabelScore(
    const skygate::ephemeris::CelestialBody& body,
    const SkyRenderGlyph& glyph,
    const double viewportWidth,
    const double viewportHeight
)
{
    double score = 0.0;
    if (isMessierObject(body)) {
        score += 1000.0;
    } else if (hasCommonNameAlias(body)) {
        score += 700.0;
    }

    if (std::isfinite(body.visualMagnitude)) {
        score += std::clamp(14.0 - body.visualMagnitude, 0.0, 16.0) * 12.0;
    }

    if (body.deepSkyObject.has_value()) {
        score += std::clamp(
            body.deepSkyObject->majorAxisArcmin.value_or(0.0),
            0.0,
            120.0
        ) * 0.75;
    }

    const double centerX = viewportWidth * 0.5;
    const double centerY = viewportHeight * 0.5;
    const double normalizedDistanceSquared =
        skygate::core::squaredDistance2d(
            glyph.x,
            glyph.y,
            centerX,
            centerY
        ) / std::max(1.0, viewportWidth * viewportWidth + viewportHeight * viewportHeight);

    return score - (normalizedDistanceSquared * 120.0);
}

[[nodiscard]] skygate::core::Rect2d labelBounds(
    const double anchorX,
    const double anchorY,
    const std::string_view text
)
{
    constexpr double kLabelPaddingX = 12.0;
    constexpr double kLabelHeightPx = 20.0;
    constexpr double kLabelOffsetYPx = 8.0;
    constexpr double kApproximateGlyphWidthPx = 6.8;

    const double width = std::max(
        24.0,
        (static_cast<double>(text.size()) * kApproximateGlyphWidthPx) + kLabelPaddingX
    );
    const double left = anchorX - (width * 0.5);
    const double top = anchorY - kLabelHeightPx - kLabelOffsetYPx;
    return skygate::core::Rect2d {
        .left = left,
        .top = top,
        .right = left + width,
        .bottom = top + kLabelHeightPx
    };
}

[[nodiscard]] bool labelFitsViewport(
    const skygate::core::Rect2d& bounds,
    const double viewportWidth,
    const double viewportHeight,
    const double edgeMarginPx
)
{
    return skygate::core::fitsWithin(bounds, viewportWidth, viewportHeight, edgeMarginPx);
}

void appendLabel(
    QVariantList& labels,
    const double x,
    const double y,
    const std::string_view text,
    const QColor& color
)
{
    QVariantMap labelEntry;
    labelEntry.insert("x", x);
    labelEntry.insert("y", y);
    labelEntry.insert("text", QString::fromStdString(std::string(text)));
    labelEntry.insert("color", color);
    labels.push_back(std::move(labelEntry));
}

void appendDeepSkyLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::ProjectionParams& projectionParams,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
)
{
    const std::size_t labelBudget = deepSkyLabelBudget(
        projectionParams.fovDeg,
        viewportWidth,
        viewportHeight
    );
    if (labelBudget == 0U) {
        return;
    }

    std::vector<DeepSkyLabelCandidate> candidates;
    candidates.reserve(std::min(frame.glyphs.size(), labelBudget * 4U));
    for (std::size_t glyphIndex = 0; glyphIndex < frame.glyphs.size(); ++glyphIndex) {
        const auto& glyph = frame.glyphs[glyphIndex];
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (
            body.displayName.empty()
            || seenLabels.contains(body.displayName)
            || !shouldConsiderDeepSkyLabel(body, projectionParams.fovDeg)
        ) {
            continue;
        }

        const double anchorY = glyph.y - deepSkyHitRadius(glyph);
        const skygate::core::Rect2d bounds = labelBounds(glyph.x, anchorY, body.displayName);
        if (!labelFitsViewport(bounds, viewportWidth, viewportHeight, edgeMarginPx)) {
            continue;
        }

        candidates.push_back(DeepSkyLabelCandidate {
            .glyphIndex = glyphIndex,
            .score = deepSkyLabelScore(body, glyph, viewportWidth, viewportHeight),
            .bounds = bounds
        });
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&frame, &snapshot](
            const DeepSkyLabelCandidate& lhs,
            const DeepSkyLabelCandidate& rhs
        ) {
            if (std::abs(lhs.score - rhs.score) > 1e-9) {
                return lhs.score > rhs.score;
            }

            const auto& lhsBody = snapshot.bodyAt(frame.glyphs[lhs.glyphIndex].bodyIndex);
            const auto& rhsBody = snapshot.bodyAt(frame.glyphs[rhs.glyphIndex].bodyIndex);
            return lhsBody.displayName < rhsBody.displayName;
        }
    );

    std::size_t acceptedCount = 0U;
    for (const DeepSkyLabelCandidate& candidate : candidates) {
        if (acceptedCount >= labelBudget) {
            break;
        }

        if (labelGrid.collides(candidate.bounds)) {
            continue;
        }

        const auto& glyph = frame.glyphs[candidate.glyphIndex];
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (!seenLabels.insert(body.displayName).second) {
            continue;
        }

        labelGrid.add(candidate.bounds);
        appendLabel(
            frame.labels,
            glyph.x,
            glyph.y - deepSkyHitRadius(glyph),
            body.displayName,
            labelColorForBodyType(body.type, renderTheme)
        );
        ++acceptedCount;
    }
}

}  // namespace

SkyRenderFrame SkyRenderFrameBuilder::buildFrame(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::PreparedProjection& projection,
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const double magnitudeCutoff,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers
) const
{
    SkyRenderFrame frame;
    frame.lines.reserve(lineRefs.size());

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
    std::optional<HorizontalLookup> lookup;
    if (!lineRefs.empty() || !labelRefs.empty()) {
        lookup.emplace(lineRefs, labelRefs);
    }

    for (const auto& state : snapshot.states) {
        if (!state.horizontal.isFinite()) {
            continue;
        }

        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (lookup.has_value()) {
            lookup->capture(body, state.horizontal);
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

    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.90;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;

    if (lookup.has_value() && overlayLayers.constellationLines) {
        const skygate::core::ProjectedPolylineBuilder lineBuilder;
        const QColor lineColor = SkyContextRenderStyle::constellationLineColor(renderTheme);
        for (const auto& lineRef : lineRefs) {
            const auto* startHorizontal = lookup->findHorizontal(lineRef.first);
            const auto* endHorizontal = lookup->findHorizontal(lineRef.second);
            if (startHorizontal == nullptr || endHorizontal == nullptr) {
                continue;
            }

            const std::array<skygate::core::HorizontalCoordinate, 2> coordinates {
                *startHorizontal,
                *endHorizontal
            };
            for (const auto& segment : lineBuilder.build(
                     projection,
                     coordinates,
                     maxSegmentLengthSquared
                 )) {
                frame.lines.push_back(SkyRenderLine {
                    .x1 = segment.x1,
                    .y1 = segment.y1,
                    .x2 = segment.x2,
                    .y2 = segment.y2,
                    .widthPx = kConstellationLineWidthPx,
                    .color = lineColor
                });
            }
        }
    }

    std::unordered_set<std::string_view> seenLabels;
    frame.labels.reserve(static_cast<int>(labelRefs.size() + 16U));
    constexpr double kEdgeMarginPx = 10.0;
    skygate::core::RectOccupancyGrid labelGrid(72.0);

    for (const auto& point : frame.points) {
        const auto& body = snapshot.bodyAt(point.bodyIndex);
        const bool isConstellationLabel =
            body.type == skygate::ephemeris::CelestialBodyType::Constellation;
        const bool isSolarSystemLabel =
            body.type == skygate::ephemeris::CelestialBodyType::Planet
            || body.type == skygate::ephemeris::CelestialBodyType::Sun
            || body.type == skygate::ephemeris::CelestialBodyType::Moon;
        if (
            (!isConstellationLabel || !overlayLayers.constellationLabels)
            && (!isSolarSystemLabel || !overlayLayers.solarSystemLabels)
        ) {
            continue;
        }

        if (body.displayName.empty() || !seenLabels.insert(body.displayName).second) {
            continue;
        }

        if (
            point.x < kEdgeMarginPx
            || point.x > (viewportWidth - kEdgeMarginPx)
            || point.y < kEdgeMarginPx
            || point.y > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        const skygate::core::Rect2d bounds = labelBounds(point.x, point.y, body.displayName);
        appendLabel(
            frame.labels,
            point.x,
            point.y,
            body.displayName,
            labelColorForBodyType(body.type, renderTheme)
        );
        labelGrid.add(bounds);
    }

    if (overlayLayers.deepSkyLabels) {
        appendDeepSkyLabels(
            frame,
            snapshot,
            projection.params(),
            viewportWidth,
            viewportHeight,
            renderTheme,
            kEdgeMarginPx,
            seenLabels,
            labelGrid
        );
    }

    if (lookup.has_value() && overlayLayers.constellationLabels) {
        for (const auto& labelRef : labelRefs) {
            if (labelRef.first.empty() || labelRef.second.empty()) {
                continue;
            }

            if (!seenLabels.insert(labelRef.first).second) {
                continue;
            }

            double sumX = 0.0;
            double sumY = 0.0;
            int visiblePointCount = 0;
            for (const std::string& hipId : labelRef.second) {
                const auto* horizontal = lookup->findHorizontal(hipId);
                if (horizontal == nullptr) {
                    continue;
                }

                const auto projected = projection.project(*horizontal);
                if (!projected.isVisible || !projected.isFinite()) {
                    continue;
                }

                sumX += projected.x;
                sumY += projected.y;
                ++visiblePointCount;
            }

            if (visiblePointCount == 0) {
                continue;
            }

            const double labelX = sumX / static_cast<double>(visiblePointCount);
            const double labelY = sumY / static_cast<double>(visiblePointCount);
            const skygate::core::Rect2d bounds = labelBounds(labelX, labelY, labelRef.first);
            if (
                !labelFitsViewport(bounds, viewportWidth, viewportHeight, kEdgeMarginPx)
                || labelGrid.collides(bounds)
            ) {
                continue;
            }

            appendLabel(
                frame.labels,
                labelX,
                labelY,
                labelRef.first,
                labelColorForBodyType(
                    skygate::ephemeris::CelestialBodyType::Constellation,
                    renderTheme
                )
            );
            labelGrid.add(bounds);
        }
    }

    return frame;
}
