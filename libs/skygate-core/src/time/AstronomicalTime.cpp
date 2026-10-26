#include "AstronomicalTime.hpp"
#include "EpochCodec.hpp"
#include "math/AngleMath.hpp"
#include "math/TimeConstants.hpp"

namespace skygate::core {

using skygate::core::TimeConstants;

double AstronomicalTime::meanObliquityDeg(const double daysSinceJ2000) noexcept
{
    return 23.4393 - 0.0000004 * daysSinceJ2000;
}

double AstronomicalTime::greenwichMeanSiderealTimeDeg(const UtcTimePoint& utcTime) noexcept
{
    const double julianDay = EpochCodec::julianDayFromUtc(utcTime);
    return AngleMath::normalizeDegrees(280.46061837 + 360.98564736629 * (julianDay - TimeConstants::kJulianDateJ2000));
}

double AstronomicalTime::localSiderealTimeDeg(const UtcTimePoint& utcTime, const double longitudeDeg) noexcept
{
    return AngleMath::normalizeDegrees(greenwichMeanSiderealTimeDeg(utcTime) + longitudeDeg);
}

double AstronomicalTime::hourAngleDeg(
    const double rightAscensionHours, const UtcTimePoint& utcTime, const double longitudeDeg
) noexcept
{
    return AngleMath::normalizeDegreesSigned(
        localSiderealTimeDeg(utcTime, longitudeDeg) - AngleMath::hoursToDegrees(rightAscensionHours)
    );
}

}  // namespace skygate::core
