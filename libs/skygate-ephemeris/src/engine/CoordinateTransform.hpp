#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::ephemeris {

class CoordinateTransform final {
public:
    [[nodiscard]] static core::EquatorialCoordinate eclipticToEquatorial(
        double eclipticLongitudeDeg,
        double eclipticLatitudeDeg,
        double obliquityDeg
    ) noexcept;

    [[nodiscard]] static core::HorizontalCoordinate equatorialToHorizontal(
        const core::EquatorialCoordinate& equatorial,
        const core::GeoLocation& observer,
        const core::UtcTimePoint& utcTime
    ) noexcept;
};

}  // namespace skygate::ephemeris
