#pragma once

#include "CelestialReferenceCalculator.hpp"
#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "UtcTimePoint.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>
#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>

namespace skygate::ui::internal {

// The reference circles of the sky overlay. The viewport geometry samples them
// into line segments and the overlay labels sample the same points for their
// placement, so one table owns the layer flag, the palette color, the label,
// and the sampled points.
class SkyReferenceOverlayLayers final {
public:
    enum class Circle : std::uint8_t {
        Ecliptic = 0,
        CelestialEquator = 1,
        CircumpolarBoundary = 2,
    };

    struct Layer final {
        Circle circle = Circle::Ecliptic;
        bool enabled = false;
        QColor color;
        QString label;
    };

    // Samples per full reference circle, shared by the geometry and the labels.
    static constexpr int kSampleCount = skygate::ephemeris::CelestialReferenceCalculator::kReferenceCircleSamples;
    static constexpr std::size_t kLayerCount = 3;

    SkyReferenceOverlayLayers() = delete;

    [[nodiscard]] static std::array<Layer, kLayerCount> layers(
        const SkyThemeRenderPalette& renderTheme,
        const SkyOverlayLayerVisibility& overlayLayers,
        const skygate::core::GeoLocation& observer
    );

    [[nodiscard]] static skygate::core::HorizontalCoordinate samplePoint(
        Circle circle,
        int sampleIndex,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;
};

}  // namespace skygate::ui::internal
