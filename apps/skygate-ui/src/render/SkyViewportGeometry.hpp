#pragma once

#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyTheme.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/SkyTypes.hpp"

#include <QColor>

#include <span>
#include <vector>

namespace skygate::ui::internal {

struct SkyViewportLineSegment final {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
    float widthPx = 1.0F;
    QColor color;
};

struct SkyViewportGeometryInput final {
    const skygate::core::PreparedProjection& projection;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    const SkyThemeRenderPalette& renderTheme;
    const SkyOverlayLayerVisibility& overlayLayers;
    skygate::core::GeoLocation observer;
    skygate::core::UtcTimePoint utcTime {};
    std::span<const SkyRenderLine> renderLines;
    std::span<const SkyRenderGlyph> renderGlyphs;
};

[[nodiscard]] std::vector<SkyViewportLineSegment> buildSkyViewportLineSegments(
    const SkyViewportGeometryInput& input
);

}  // namespace skygate::ui::internal
