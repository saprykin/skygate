#pragma once

#include "math/Matrix3x3.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

class ErfaAstrometry final {
public:
    [[nodiscard]] static std::optional<skygate::core::Matrix3x3>
    celestialToIntermediateMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<skygate::core::Matrix3x3>
    precessionNutationMatrix06A(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<skygate::core::Matrix3x3>
    earthRotationMatrix00(const AstronomicalEpoch& universalTime1) noexcept;
    [[nodiscard]] static std::optional<double> tioLocatorS00(const AstronomicalEpoch& terrestrialTime) noexcept;
    [[nodiscard]] static std::optional<skygate::core::Matrix3x3>
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
