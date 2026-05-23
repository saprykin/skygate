#include "engine/simple/AstronomicalTime.hpp"
#include "math/TimeConstants.hpp"

#include "UtcTimeCodec.hpp"
#include "math/AngleMath.hpp"

#include <cmath>

namespace skygate::ephemeris {

using core::TimeConstants;

bool AstronomicalTime::hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && (epoch.julianDatePart1 != 0.0 || epoch.julianDatePart2 != 0.0);
}

AstronomicalEpoch AstronomicalTime::epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
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

core::UtcTimePoint AstronomicalTime::utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = normalizedAstronomicalEpoch(epoch);
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double epochMicros =
        std::round((julianDay - TimeConstants::kJulianDateUnixEpoch) * TimeConstants::kSecondsPerDay * 1'000'000.0);
    return core::UtcTimeCodec::fromEpochMicros(static_cast<std::int64_t>(epochMicros));
}

double AstronomicalTime::julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept
{
    return core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / TimeConstants::kSecondsPerDay
           + TimeConstants::kJulianDateUnixEpoch;
}

double AstronomicalTime::daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept
{
    return julianDayFromUtc(utcTime) - TimeConstants::kJulianDateJ2000;
}

double AstronomicalTime::meanObliquityDeg(const double daysSinceJ2000) noexcept
{
    return 23.4393 - 0.0000004 * daysSinceJ2000;
}

double AstronomicalTime::greenwichMeanSiderealTimeDeg(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay = julianDayFromUtc(utcTime);
    return core::AngleMath::normalizeDegrees(
        280.46061837 + 360.98564736629 * (julianDay - TimeConstants::kJulianDateJ2000)
    );
}

}  // namespace skygate::ephemeris
