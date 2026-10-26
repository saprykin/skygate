#include "SkyRenderStylePolicy.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <algorithm>

namespace skygate::ui::internal {
namespace {

constexpr double kDefaultMajorAxisArcmin = 10.0;
constexpr double kGlyphRadiusMinPx = 4.5;
constexpr double kGlyphRadiusMaxPx = 44.0;
constexpr double kGlyphRadiusYMinPx = 4.0;
constexpr double kGlyphRadiusYMaxPx = 32.0;
constexpr double kPlanetaryNebulaRadiusMinPx = 5.0;
constexpr double kPlanetaryNebulaRadiusMaxPx = 12.0;

}  // namespace

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

SkyRenderStylePolicy::GlyphRadii SkyRenderStylePolicy::deepSkyGlyphRadiiPx(
    const skygate::ephemeris::BaseCelestialBody& body, const skygate::core::ProjectionParams& projectionParams
)
{
    const skygate::ephemeris::DeepSkyObjectInfo defaultInfo;
    const auto& info = body.deepSkyObjectValue().has_value() ? *body.deepSkyObjectValue() : defaultInfo;
    const double pixelsPerDeg = std::min(projectionParams.viewportWidth, projectionParams.viewportHeight)
                                / std::max(1.0, projectionParams.fovDeg);
    const double majorArcmin = info.majorAxisArcmin.value_or(kDefaultMajorAxisArcmin);
    const double minorArcmin = info.minorAxisArcmin.value_or(majorArcmin);
    const double radiusX = ((majorArcmin / 60.0) * pixelsPerDeg) * 0.5;
    const double radiusY = ((minorArcmin / 60.0) * pixelsPerDeg) * 0.5;

    GlyphRadii radii{
        .radiusXPx = std::clamp(radiusX, kGlyphRadiusMinPx, kGlyphRadiusMaxPx),
        .radiusYPx = std::clamp(radiusY, kGlyphRadiusYMinPx, kGlyphRadiusYMaxPx)
    };
    if (info.kind == skygate::ephemeris::DeepSkyObjectInfo::Kind::PlanetaryNebula) {
        radii.radiusXPx = std::clamp(radiusX, kPlanetaryNebulaRadiusMinPx, kPlanetaryNebulaRadiusMaxPx);
        radii.radiusYPx = radii.radiusXPx;
    }

    return radii;
}

}  // namespace skygate::ui::internal
