#pragma once

#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class AstronomicalTime final {
public:
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
    [[nodiscard]] static double greenwichMeanSiderealTimeDeg(const skygate::core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
