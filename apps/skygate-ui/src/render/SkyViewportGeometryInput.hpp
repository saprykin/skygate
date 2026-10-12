#pragma once

#include "GeoLocation.hpp"
#include "PreparedProjection.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderGlyph.hpp"
#include "SkyRenderLine.hpp"
#include "SkyThemeRenderPalette.hpp"
#include "UtcTimePoint.hpp"

#include <span>

namespace skygate::ui::internal {

struct SkyViewportGeometryInput final {
    const skygate::core::PreparedProjection& projection;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    const SkyThemeRenderPalette& renderTheme;
    const SkyOverlayLayerVisibility& overlayLayers;
    skygate::core::GeoLocation observer;
    skygate::core::UtcTimePoint utcTime{};
    std::span<const SkyRenderLine> renderLines;
    std::span<const SkyRenderGlyph> renderGlyphs;
};

}  // namespace skygate::ui::internal
