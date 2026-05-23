#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"
#include "math/AngleMath.hpp"
#include "math/TimeConstants.hpp"

namespace skygate::ephemeris {

using core::TimeConstants;

double AstronomicalTime::meanObliquityDeg(const double daysSinceJ2000) noexcept
{
    return 23.4393 - 0.0000004 * daysSinceJ2000;
}

double AstronomicalTime::greenwichMeanSiderealTimeDeg(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay = EpochCodec::julianDayFromUtc(utcTime);
    return core::AngleMath::normalizeDegrees(
        280.46061837 + 360.98564736629 * (julianDay - TimeConstants::kJulianDateJ2000)
    );
}

}  // namespace skygate::ephemeris
