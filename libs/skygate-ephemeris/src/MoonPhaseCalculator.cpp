#include "MoonPhaseCalculator.hpp"
#include "math/MathConstants.hpp"
#include "math/PhysicalConstants.hpp"
#include "math/TimeConstants.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] double normalizedLunarCycleFraction(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = epoch.normalized();
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double daysSinceKnownNewMoon = julianDay - skygate::core::TimeConstants::kJulianDateKnownNewMoon;
    double fraction = std::fmod(daysSinceKnownNewMoon / skygate::core::PhysicalConstants::kSynodicMonthDays, 1.0);
    if (fraction < 0.0) {
        fraction += 1.0;
    }
    return fraction;
}

[[nodiscard]] double moonIlluminationPercent(const double lunarCycleFraction) noexcept
{
    return std::clamp((1.0 - std::cos(skygate::core::MathConstants::kTwoPi * lunarCycleFraction)) * 50.0, 0.0, 100.0);
}

[[nodiscard]] std::string moonPhaseName(const double lunarCycleFraction)
{
    if (lunarCycleFraction < 1.0 / 16.0 || lunarCycleFraction >= 15.0 / 16.0) {
        return "New Moon";
    }
    if (lunarCycleFraction < 3.0 / 16.0) {
        return "Waxing crescent";
    }
    if (lunarCycleFraction < 5.0 / 16.0) {
        return "First quarter";
    }
    if (lunarCycleFraction < 7.0 / 16.0) {
        return "Waxing gibbous";
    }
    if (lunarCycleFraction < 9.0 / 16.0) {
        return "Full Moon";
    }
    if (lunarCycleFraction < 11.0 / 16.0) {
        return "Waning gibbous";
    }
    if (lunarCycleFraction < 13.0 / 16.0) {
        return "Last quarter";
    }
    return "Waning crescent";
}

}  // namespace

MoonPhase MoonPhaseCalculator::compute(const AstronomicalEpoch& epoch) const noexcept
{
    const double fraction = normalizedLunarCycleFraction(epoch);
    return MoonPhase{
        .illuminationPercent = moonIlluminationPercent(fraction),
        .phaseName = moonPhaseName(fraction),
    };
}

}  // namespace skygate::ephemeris
