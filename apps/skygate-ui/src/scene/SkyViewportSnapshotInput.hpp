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
#include <span>

struct SkyViewportSnapshotInput final {
    std::uint64_t renderFrameGeneration = 0;
    std::optional<std::uint32_t> trailTargetBodyIndex;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    skygate::core::ObservationContext observationContext;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    SkyOverlayLayerVisibility overlayLayers;
    std::span<const SkyRenderPoint> points;
    std::span<const SkyRenderLine> lines;
    std::span<const SkyRenderGlyph> glyphs;
};
