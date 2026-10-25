#pragma once

#include "BaseCelestialBody.hpp"
#include "ProjectionParams.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>

namespace skygate::ui::internal {

class SkyRenderStylePolicy final {
public:
    struct GlyphRadii final {
        double radiusXPx = 0.0;
        double radiusYPx = 0.0;
    };

    [[nodiscard]] static double pointSizeForMagnitude(double magnitude);
    [[nodiscard]] static QColor
    colorForBodyType(skygate::ephemeris::BaseCelestialBody::Kind type, const SkyThemeRenderPalette& renderPalette);
    [[nodiscard]] static GlyphRadii deepSkyGlyphRadiiPx(
        const skygate::ephemeris::BaseCelestialBody& body, const skygate::core::ProjectionParams& projectionParams
    );
};

}  // namespace skygate::ui::internal
