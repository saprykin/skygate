#pragma once

#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class CelestialReferenceCalculator final {
public:
    // Samples per full reference circle. Rendering and label placement use the
    // same count so both see the same circle points.
    static constexpr int kReferenceCircleSamples = 96;

    [[nodiscard]] static skygate::core::HorizontalCoordinate eclipticPoint(
        double eclipticLongitudeDeg,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static skygate::core::HorizontalCoordinate eclipticCirclePoint(
        int sampleIndex,
        int sampleCount,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static skygate::core::HorizontalCoordinate equatorialPoint(
        double rightAscensionHours,
        double declinationDeg,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static skygate::core::HorizontalCoordinate declinationCirclePoint(
        int sampleIndex,
        int sampleCount,
        double declinationDeg,
        const skygate::core::GeoLocation& observer,
        const skygate::core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static double circumpolarBoundaryDeclinationDeg(const skygate::core::GeoLocation& observer) noexcept;
};

}  // namespace skygate::ephemeris
