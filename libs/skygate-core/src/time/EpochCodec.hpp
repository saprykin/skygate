#pragma once

#include "AstronomicalEpoch.hpp"
#include "UtcTimePoint.hpp"

#include <cstdint>

namespace skygate::core {

class EpochCodec final {
public:
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTime(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTimeExact(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static AstronomicalEpoch epochFromNtpTimestamp(std::uint64_t ntpTimestamp) noexcept;
    [[nodiscard]] static double julianDayFromUtc(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double julianDayFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static double julianDateFromDaysSinceUnixEpoch(double daysSinceUnix) noexcept;
    [[nodiscard]] static double daysSinceUnixEpoch(double julianPart1, double julianPart2) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double daysSinceJ2000(const AstronomicalEpoch& epoch) noexcept;
};

}  // namespace skygate::core
