#include "AstronomicalEpoch.hpp"
#include "math/TimeConstants.hpp"

#include <cmath>

namespace skygate::ephemeris {

AstronomicalEpoch AstronomicalEpoch::normalized() const noexcept
{
    if (!std::isfinite(julianDatePart1) || !std::isfinite(julianDatePart2)) {
        return *this;
    }

    const double part1Whole = std::floor(julianDatePart1);
    const double part2WithPart1Fraction = (julianDatePart1 - part1Whole) + julianDatePart2;
    const double part2Whole = std::floor(part2WithPart1Fraction);
    return AstronomicalEpoch{
        .julianDatePart1 = part1Whole + part2Whole,
        .julianDatePart2 = part2WithPart1Fraction - part2Whole,
        .timeScale = timeScale,
    };
}

bool AstronomicalEpoch::hasExplicit() const noexcept
{
    return std::isfinite(julianDatePart1) && std::isfinite(julianDatePart2)
           && (julianDatePart1 != 0.0 || julianDatePart2 != 0.0);
}

bool AstronomicalEpoch::isFinite() const noexcept
{
    return std::isfinite(julianDatePart1) && std::isfinite(julianDatePart2);
}

bool AstronomicalEpoch::isFiniteUtc() const noexcept
{
    return timeScale == TimeScale::Utc && std::isfinite(julianDatePart1) && std::isfinite(julianDatePart2);
}

double AstronomicalEpoch::sortKey() const noexcept
{
    const AstronomicalEpoch normalizedEpoch = normalized();
    return normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
}

AstronomicalEpoch AstronomicalEpoch::addSeconds(const double offsetSeconds) const noexcept
{
    return addSeconds(offsetSeconds, timeScale);
}

AstronomicalEpoch AstronomicalEpoch::addSeconds(const double offsetSeconds, const TimeScale resultScale) const noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDatePart2 + offsetSeconds / skygate::core::TimeConstants::kSecondsPerDay,
        .timeScale = resultScale
    }
        .normalized();
}

AstronomicalEpoch AstronomicalEpoch::addMinutes(const double offsetMinutes) const noexcept
{
    return addMinutes(offsetMinutes, timeScale);
}

AstronomicalEpoch AstronomicalEpoch::addMinutes(const double offsetMinutes, const TimeScale resultScale) const noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDatePart2 + offsetMinutes / skygate::core::TimeConstants::kMinutesPerDay,
        .timeScale = resultScale
    }
        .normalized();
}

}  // namespace skygate::ephemeris
