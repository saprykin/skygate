#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyTheme.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <span>

class SkyRenderFrameBuilder final {
public:
    [[nodiscard]] SkyRenderFrame buildFrame(
        const skygate::ephemeris::EphemerisSnapshot& snapshot,
        const skygate::core::PreparedProjection& projection,
        std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs,
        std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups,
        double magnitudeCutoff,
        double viewportWidth,
        double viewportHeight,
        const skygate::ui::internal::SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers
    ) const;
};
