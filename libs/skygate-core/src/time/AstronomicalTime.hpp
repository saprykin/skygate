#pragma once

#include "UtcTimePoint.hpp"

namespace skygate::core {

class AstronomicalTime final {
public:
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
    [[nodiscard]] static double greenwichMeanSiderealTimeDeg(const UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::core
