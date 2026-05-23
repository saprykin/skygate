#include "time/AstronomicalEpoch.hpp"

#include <cmath>

namespace skygate::ephemeris {

AstronomicalEpoch normalizedAstronomicalEpoch(const AstronomicalEpoch& epoch) noexcept
{
    if (!std::isfinite(epoch.julianDatePart1) || !std::isfinite(epoch.julianDatePart2)) {
        return epoch;
    }

    const double part1Whole = std::floor(epoch.julianDatePart1);
    const double part2WithPart1Fraction = (epoch.julianDatePart1 - part1Whole) + epoch.julianDatePart2;
    const double part2Whole = std::floor(part2WithPart1Fraction);
    return AstronomicalEpoch{
        .julianDatePart1 = part1Whole + part2Whole,
        .julianDatePart2 = part2WithPart1Fraction - part2Whole,
        .timeScale = epoch.timeScale,
    };
}

bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && (epoch.julianDatePart1 != 0.0 || epoch.julianDatePart2 != 0.0);
}

bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

bool isFiniteUtcEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.timeScale == TimeScale::Utc && std::isfinite(epoch.julianDatePart1)
           && std::isfinite(epoch.julianDatePart2);
}

double epochSortKey(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalized = normalizedAstronomicalEpoch(epoch);
    return normalized.julianDatePart1 + normalized.julianDatePart2;
}

}  // namespace skygate::ephemeris
