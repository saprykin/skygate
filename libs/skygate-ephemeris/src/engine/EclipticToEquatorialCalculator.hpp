#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::ephemeris {

class EclipticToEquatorialCalculator final {
public:
    [[nodiscard]] static core::EquatorialCoordinate eclipticToEquatorial(
        double eclipticLongitudeDeg,
        double eclipticLatitudeDeg,
        double obliquityDeg
    ) noexcept;
};

}  // namespace skygate::ephemeris
