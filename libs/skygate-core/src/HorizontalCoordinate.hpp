#pragma once

#include "math/MathConstants.hpp"

namespace skygate::core {

struct HorizontalCoordinate {
    static constexpr double kAltitudeMinDeg = -MathConstants::kPolarAngleDeg;
    static constexpr double kAltitudeMaxDeg = MathConstants::kPolarAngleDeg;

    double altitudeDeg = 0.0;
    double azimuthDeg = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] HorizontalCoordinate normalizedAzimuth() const noexcept;
};

}  // namespace skygate::core
