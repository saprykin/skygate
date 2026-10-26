#pragma once

#include "math/MathConstants.hpp"

namespace skygate::core {

struct GeoLocation {
    static constexpr double kLatitudeMinDeg = -MathConstants::kPolarAngleDeg;
    static constexpr double kLatitudeMaxDeg = MathConstants::kPolarAngleDeg;
    static constexpr double kLongitudeMinDeg = -180.0;
    static constexpr double kLongitudeMaxDeg = 180.0;

    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool equals(const GeoLocation& other) const noexcept;

    // Bit identity for cache keys: +0.0 and -0.0 differ here even though
    // equals() treats them as equal.
    [[nodiscard]] bool isIdenticalTo(const GeoLocation& other) const noexcept;
};

}  // namespace skygate::core
