#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::ephemeris {

class CelestialReferenceCalculator final {
public:
    [[nodiscard]] static core::HorizontalCoordinate eclipticPoint(
        double eclipticLongitudeDeg,
        const core::GeoLocation& observer,
        const core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static core::HorizontalCoordinate equatorialPoint(
        double rightAscensionHours,
        double declinationDeg,
        const core::GeoLocation& observer,
        const core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static core::HorizontalCoordinate declinationCirclePoint(
        int sampleIndex,
        int sampleCount,
        double declinationDeg,
        const core::GeoLocation& observer,
        const core::UtcTimePoint& utcTime
    ) noexcept;

    [[nodiscard]] static core::HorizontalCoordinate meridianPoint(double progress) noexcept;

    [[nodiscard]] static double circumpolarBoundaryDeclinationDeg(
        const core::GeoLocation& observer
    ) noexcept;
};

}  // namespace skygate::ephemeris
