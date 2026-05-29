#pragma once

#include "time/AstronomicalEpoch.hpp"

#include <array>
#include <optional>

namespace skygate::ephemeris::highprecision {

using Matrix3x3 = std::array<std::array<double, 3>, 3>;

class ErfaAstrometry final {
public:
    [[nodiscard]] static std::optional<Matrix3x3>
    celestialToIntermediateMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<Matrix3x3>
    precessionNutationMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<double> earthRotationAngle00(const AstronomicalEpoch& universalTime1) noexcept;
    [[nodiscard]] static std::optional<double> tioLocatorS00(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<Matrix3x3>
    polarMotionMatrix00(double polarMotionXRadians, double polarMotionYRadians, double tioLocatorRadians) noexcept;
    [[nodiscard]] static std::optional<double> tdbMinusTtSeconds(
        const AstronomicalEpoch& terrestrialTime,
        double ut1FractionOfDay,
        double eastLongitudeRadians = 0.0,
        double distanceFromSpinAxisKm = 0.0,
        double distanceNorthOfEquatorialPlaneKm = 0.0
    ) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
