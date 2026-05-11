#pragma once

#include "SkyRenderFrame.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyTheme.hpp"
#include "skygate/core/PreparedProjection.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <span>

class SkyRenderFrameBuilder final {
public:
    [[nodiscard]] SkyRenderFrame buildFrame(
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::PreparedProjection& projection,
        std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
        double magnitudeCutoff,
        double viewportWidth,
        double viewportHeight,
        const skygate::ui::internal::SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers
    ) const;
};
