#pragma once

#include "AstronomicalEpoch.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::core {

class EpochCodec final {
public:
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTime(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static double julianDayFromUtc(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::core
