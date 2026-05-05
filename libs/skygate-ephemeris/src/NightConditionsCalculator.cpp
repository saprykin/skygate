#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include "engine/JulianDateTime.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::ephemeris {
namespace {

constexpr double kSunriseSunsetAltitudeDeg = -0.833;
constexpr double kCivilTwilightAltitudeDeg = -6.0;
constexpr double kNauticalTwilightAltitudeDeg = -12.0;
constexpr double kAstronomicalTwilightAltitudeDeg = -18.0;
constexpr double kKnownNewMoonJulianDay = 2451550.1;
constexpr double kSynodicMonthDays = 29.530588853;
constexpr double kTwoPi = 6.28318530717958647692;

[[nodiscard]] ObservationEvent unavailableEvent() noexcept
{
    return ObservationEvent {
        .status = ObservationEventStatus::Unresolved
    };
}

[[nodiscard]] double normalizedLunarCycleFraction(const core::UtcTimePoint& utcTime) noexcept
{
    const double daysSinceKnownNewMoon =
        JulianDateTime::julianDayFromUtc(utcTime) - kKnownNewMoonJulianDay;
    double fraction = std::fmod(daysSinceKnownNewMoon / kSynodicMonthDays, 1.0);
    if (fraction < 0.0) {
        fraction += 1.0;
    }
    return fraction;
}

[[nodiscard]] double moonIlluminationPercent(const double lunarCycleFraction) noexcept
{
    return std::clamp(
        (1.0 - std::cos(kTwoPi * lunarCycleFraction)) * 50.0,
        0.0,
        100.0
    );
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

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t sunBodyIndex,
    const std::uint32_t moonBodyIndex
) const
{
    NightConditions conditions;
    conditions.sunrise = unavailableEvent();
    conditions.sunset = unavailableEvent();
    conditions.civilDawn = unavailableEvent();
    conditions.civilDusk = unavailableEvent();
    conditions.nauticalDawn = unavailableEvent();
    conditions.nauticalDusk = unavailableEvent();
    conditions.astronomicalDawn = unavailableEvent();
    conditions.astronomicalDusk = unavailableEvent();
    conditions.moonrise = unavailableEvent();
    conditions.moonset = unavailableEvent();

    if (!context.observer.isValid()) {
        return conditions;
    }

    const auto sunState = ephemerisEngine.computeBodyState(context, sunBodyIndex);
    const auto moonState = ephemerisEngine.computeBodyState(context, moonBodyIndex);
    if (
        !sunState.has_value()
        || !sunState->horizontal.isFinite()
        || !moonState.has_value()
        || !moonState->horizontal.isFinite()
    ) {
        return conditions;
    }

    const ObservationEventCalculator eventCalculator;
    const auto sunHorizon = eventCalculator.compute(
        ephemerisEngine,
        context,
        sunBodyIndex,
        kSunriseSunsetAltitudeDeg
    );
    const auto civil = eventCalculator.compute(
        ephemerisEngine,
        context,
        sunBodyIndex,
        kCivilTwilightAltitudeDeg
    );
    const auto nautical = eventCalculator.compute(
        ephemerisEngine,
        context,
        sunBodyIndex,
        kNauticalTwilightAltitudeDeg
    );
    const auto astronomical = eventCalculator.compute(
        ephemerisEngine,
        context,
        sunBodyIndex,
        kAstronomicalTwilightAltitudeDeg
    );
    const auto moonHorizon = eventCalculator.compute(ephemerisEngine, context, moonBodyIndex);

    const double lunarCycleFraction = normalizedLunarCycleFraction(context.utcTime);
    conditions.valid = true;
    conditions.sunAltitudeDeg = sunState->horizontal.altitudeDeg;
    conditions.sunrise = sunHorizon.nextRise;
    conditions.sunset = sunHorizon.nextSet;
    conditions.civilDawn = civil.nextRise;
    conditions.civilDusk = civil.nextSet;
    conditions.nauticalDawn = nautical.nextRise;
    conditions.nauticalDusk = nautical.nextSet;
    conditions.astronomicalDawn = astronomical.nextRise;
    conditions.astronomicalDusk = astronomical.nextSet;
    conditions.moonrise = moonHorizon.nextRise;
    conditions.moonset = moonHorizon.nextSet;
    conditions.moonIlluminationPercent = moonIlluminationPercent(lunarCycleFraction);
    conditions.moonPhaseName = moonPhaseName(lunarCycleFraction);
    return conditions;
}

}  // namespace skygate::ephemeris
