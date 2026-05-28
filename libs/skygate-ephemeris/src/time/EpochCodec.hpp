#pragma once

#include "AstronomicalEpoch.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class EpochCodec final {
public:
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static core::UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static double julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
