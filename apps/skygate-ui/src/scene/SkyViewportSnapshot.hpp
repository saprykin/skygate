#pragma once

#include "ObservationContext.hpp"
#include "PreparedProjection.hpp"
#include "render/SkyRenderGlyph.hpp"
#include "render/SkyRenderLine.hpp"
#include "render/SkyRenderPoint.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// Immutable handoff from SkySceneModel to the viewport. The model publishes a
// replacement only when projection, observation context, palette, layer
// visibility, or render primitives change; overlay-only edits leave the
// current snapshot in place.
struct SkyViewportSnapshot final {
    std::uint64_t generation = 0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    skygate::core::ObservationContext observationContext;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    SkyOverlayLayerVisibility overlayLayers;
    std::vector<SkyRenderPoint> points;
    std::vector<SkyRenderLine> lines;
    std::vector<SkyRenderGlyph> glyphs;
};
