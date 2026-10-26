#include "SkyReferenceOverlayLayers.hpp"
#include "CelestialReferenceCalculator.hpp"

#include <QString>

namespace skygate::ui::internal {

std::array<SkyReferenceOverlayLayers::Layer, SkyReferenceOverlayLayers::kLayerCount> SkyReferenceOverlayLayers::layers(
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers,
    const skygate::core::GeoLocation& observer
)
{
    return {
        Layer{
            .circle = Circle::Ecliptic,
            .enabled = overlayLayers.ecliptic,
            .color = renderTheme.eclipticLine,
            .label = QStringLiteral("Ecliptic"),
        },
        Layer{
            .circle = Circle::CelestialEquator,
            .enabled = overlayLayers.celestialEquator,
            .color = renderTheme.celestialEquatorLine,
            .label = QStringLiteral("Celestial equator"),
        },
        // The boundary needs a valid observer latitude to exist at all.
        Layer{
            .circle = Circle::CircumpolarBoundary,
            .enabled = overlayLayers.circumpolarBoundary && observer.isValid(),
            .color = renderTheme.circumpolarBoundaryLine,
            .label = QStringLiteral("Circumpolar"),
        },
    };
}

skygate::core::HorizontalCoordinate SkyReferenceOverlayLayers::samplePoint(
    const Circle circle,
    const int sampleIndex,
    const skygate::core::GeoLocation& observer,
    const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    switch (circle) {
    case Circle::Ecliptic:
        return skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(
            sampleIndex, kSampleCount, observer, utcTime
        );
    case Circle::CelestialEquator:
        return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
            sampleIndex, kSampleCount, 0.0, observer, utcTime
        );
    case Circle::CircumpolarBoundary:
        return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
            sampleIndex,
            kSampleCount,
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(observer),
            observer,
            utcTime
        );
    }

    return {};
}

}  // namespace skygate::ui::internal
