#include "EpochCodec.hpp"
#include "UtcTimeCodec.hpp"
#include "math/TimeConstants.hpp"

#include <cmath>
#include <cstdint>

namespace skygate::ephemeris {

using core::TimeConstants;

AstronomicalEpoch EpochCodec::epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay = core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / TimeConstants::kSecondsPerDay
                             + TimeConstants::kJulianDateUnixEpoch;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

core::UtcTimePoint EpochCodec::utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = epoch.normalized();
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double epochMicros =
        std::round((julianDay - TimeConstants::kJulianDateUnixEpoch) * TimeConstants::kMicrosecondsPerDay);
    return core::UtcTimeCodec::fromEpochMicros(static_cast<std::int64_t>(epochMicros));
}

double EpochCodec::julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept
{
    return core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / TimeConstants::kSecondsPerDay
           + TimeConstants::kJulianDateUnixEpoch;
}

double EpochCodec::daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept
{
    return julianDayFromUtc(utcTime) - TimeConstants::kJulianDateJ2000;
}

}  // namespace skygate::ephemeris
