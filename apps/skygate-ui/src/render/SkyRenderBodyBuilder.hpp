#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderFrame.hpp"
#include "SkyRenderHorizontalLookup.hpp"
#include "SkyTheme.hpp"

namespace skygate::ui::internal {

class SkyRenderBodyBuilder final {
public:
    void appendBodies(
        SkyRenderFrame& frame,
        SkyRenderHorizontalLookup* horizontalLookup,
        const skygate::ephemeris::EphemerisSnapshot& snapshot,
        const skygate::core::PreparedProjection& projection,
        double magnitudeCutoff,
        double viewportWidth,
        double viewportHeight,
        const SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers
    ) const;
};

}  // namespace skygate::ui::internal
