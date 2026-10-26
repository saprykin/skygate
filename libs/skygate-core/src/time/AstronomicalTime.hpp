#pragma once

#include "UtcTimePoint.hpp"

namespace skygate::core {

class AstronomicalTime final {
public:
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
    [[nodiscard]] static double greenwichMeanSiderealTimeDeg(const UtcTimePoint& utcTime) noexcept;
    // Greenwich mean sidereal time advanced by the observer's east longitude.
    [[nodiscard]] static double localSiderealTimeDeg(const UtcTimePoint& utcTime, double longitudeDeg) noexcept;
    // Local sidereal time minus the body's right ascension, in degrees.
    [[nodiscard]] static double
    hourAngleDeg(double rightAscensionHours, const UtcTimePoint& utcTime, double longitudeDeg) noexcept;
};

}  // namespace skygate::core
