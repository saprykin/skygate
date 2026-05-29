#pragma once

#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class CelestialReferenceCalculator final {
public:
    [[nodiscard]] static skygate::core::HorizontalCoordinate eclipticPoint(
        double eclipticLongitudeDeg,
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
