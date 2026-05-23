#pragma once

#include "Types.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class AstronomicalTime final {
public:
    [[nodiscard]] static bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static core::UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static double julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double meanObliquityDeg(double daysSinceJ2000) noexcept;
    [[nodiscard]] static double greenwichMeanSiderealTimeDeg(const core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
