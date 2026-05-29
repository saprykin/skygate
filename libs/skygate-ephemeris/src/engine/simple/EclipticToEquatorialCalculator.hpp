#pragma once

#include "EquatorialCoordinate.hpp"

namespace skygate::ephemeris {

class EclipticToEquatorialCalculator final {
public:
    [[nodiscard]] static skygate::core::EquatorialCoordinate
    compute(double eclipticLongitudeDeg, double eclipticLatitudeDeg, double obliquityDeg) noexcept;
};

}  // namespace skygate::ephemeris
