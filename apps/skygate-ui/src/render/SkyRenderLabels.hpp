#pragma once

#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyTheme.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/core/math/SpatialIndex2d.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <QColor>

#include <string_view>
#include <unordered_set>
#include <vector>

namespace skygate::ui::internal {

[[nodiscard]] QColor skyRenderLabelColorForBodyType(
    skygate::ephemeris::CelestialBodyType type,
    const SkyThemeRenderPalette& renderTheme
);

[[nodiscard]] skygate::core::Rect2d skyRenderLabelBounds(
    double anchorX,
    double anchorY,
    std::string_view text
);

[[nodiscard]] bool skyRenderLabelFitsViewport(
    const skygate::core::Rect2d& bounds,
    double viewportWidth,
    double viewportHeight,
    double edgeMarginPx
);

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels,
    QString kind,
    double x,
    double y,
    QString text,
    const QColor& color
);

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels,
    double x,
    double y,
    std::string_view text,
    const QColor& color
);

[[nodiscard]] double skyRenderDeepSkyHitRadius(const SkyRenderGlyph& glyph);

void appendBodyPointLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot,
    double viewportWidth,
    double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers,
    double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
);

void appendDeepSkyLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::ProjectionParams& projectionParams,
    double viewportWidth,
    double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
);

}  // namespace skygate::ui::internal
