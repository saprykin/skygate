#pragma once

#include "skygate/core/Types.hpp"

namespace skygate::ephemeris {

class JulianDateTime final {
public:
    [[nodiscard]] static double julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
    [[nodiscard]] static double greenwichMeanSiderealTimeDeg(const core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
