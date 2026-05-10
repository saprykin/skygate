#include "engine/AstronomicalTime.hpp"

#include "skygate/core/math/AngleMath.hpp"

namespace skygate::ephemeris {
namespace {

constexpr double kJ2000JulianDay = 2451545.0;
constexpr double kUnixEpochJulianDay = 2440587.5;
constexpr double kSecondsPerDay = 86400.0;

}  // namespace

double AstronomicalTime::julianDayFromUtc(const core::UtcTimePoint& utcTime) noexcept
{
    const auto epochSeconds = utcTime.time_since_epoch().count();
    return static_cast<double>(epochSeconds) / kSecondsPerDay + kUnixEpochJulianDay;
}

double AstronomicalTime::daysSinceJ2000(const core::UtcTimePoint& utcTime) noexcept
{
    return julianDayFromUtc(utcTime) - kJ2000JulianDay;
}

double AstronomicalTime::meanObliquityDeg(const double daysSinceJ2000) noexcept
{
    return 23.4393 - 0.0000004 * daysSinceJ2000;
}

double AstronomicalTime::greenwichMeanSiderealTimeDeg(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay = julianDayFromUtc(utcTime);
    return core::AngleMath::normalizeDegrees(
        280.46061837 + 360.98564736629 * (julianDay - kJ2000JulianDay)
    );
}

}  // namespace skygate::ephemeris
