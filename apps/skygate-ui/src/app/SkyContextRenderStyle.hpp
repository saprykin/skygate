#pragma once

#include "BaseCelestialBody.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>

namespace skygate::ui::internal {

class SkyContextRenderStyle final {
public:
    [[nodiscard]] static double pointSizeForMagnitude(double magnitude);
    [[nodiscard]] static QColor
    colorForBodyType(skygate::ephemeris::BaseCelestialBody::Kind type, const SkyThemeRenderPalette& renderPalette);
    [[nodiscard]] static QColor constellationLineColor(const SkyThemeRenderPalette& renderPalette);
};

}  // namespace skygate::ui::internal
