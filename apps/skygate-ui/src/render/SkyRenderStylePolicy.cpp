#include "SkyRenderStylePolicy.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <algorithm>

namespace skygate::ui::internal {

double SkyRenderStylePolicy::pointSizeForMagnitude(const double magnitude)
{
    const double normalizedBrightness = std::clamp(1.0 - ((magnitude + 1.5) / 8.0), 0.2, 1.0);
    return 1.8 + normalizedBrightness * 5.0;
}

QColor SkyRenderStylePolicy::colorForBodyType(
    const skygate::ephemeris::BaseCelestialBody::Kind type, const SkyThemeRenderPalette& renderPalette
)
{
    switch (type) {
    case skygate::ephemeris::BaseCelestialBody::Kind::Sun:
        return renderPalette.bodySun;
    case skygate::ephemeris::BaseCelestialBody::Kind::Moon:
        return renderPalette.bodyMoon;
    case skygate::ephemeris::BaseCelestialBody::Kind::Planet:
        return renderPalette.bodyPlanet;
    case skygate::ephemeris::BaseCelestialBody::Kind::Star:
        return renderPalette.bodyStar;
    case skygate::ephemeris::BaseCelestialBody::Kind::Constellation:
        return renderPalette.bodyConstellation;
    case skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject:
        return renderPalette.bodyDeepSkyObject;
    }

    return renderPalette.bodyStar;
}

QColor SkyRenderStylePolicy::constellationLineColor(const SkyThemeRenderPalette& renderPalette)
{
    return renderPalette.constellationLine;
}

}  // namespace skygate::ui::internal
