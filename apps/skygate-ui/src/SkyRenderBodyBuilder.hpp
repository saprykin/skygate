#pragma once

#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyRenderHorizontalLookup.hpp"
#include "SkyTheme.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/ephemeris/Types.hpp"

namespace skygate::ui::internal {

class SkyRenderBodyBuilder final {
public:
    void appendBodies(
        SkyRenderFrame& frame,
        SkyRenderHorizontalLookup* horizontalLookup,
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::PreparedProjection& projection,
        double magnitudeCutoff,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers
    ) const;
};

}  // namespace skygate::ui::internal
