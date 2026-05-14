#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include "engine/simple/AstronomicalTime.hpp"
#include "skygate/core/math/MathConstants.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/Types.hpp"

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
constexpr double kUnixEpochJulianDay = 2'440'587.5;
constexpr double kSecondsPerDay = 86'400.0;

[[nodiscard]] ObservationEvent unavailableEvent() noexcept
{
    return ObservationEvent{.status = ObservationEventStatus::Unresolved};
}

[[nodiscard]] AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay =
        kUnixEpochJulianDay + static_cast<double>(utcTime.time_since_epoch().count()) / kSecondsPerDay;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

[[nodiscard]] EphemerisRequest
requestFromContext(const core::SkyContext& context, const IEphemerisEngine& ephemerisEngine) noexcept
{
    EphemerisRequest request;
    request.context = context;
    request.epoch = epochFromUtcTime(context.utcTime);
    request.options = ephemerisEngine.options();
    return request;
}

[[nodiscard]] double normalizedLunarCycleFraction(const core::UtcTimePoint& utcTime) noexcept
{
    const double daysSinceKnownNewMoon = AstronomicalTime::julianDayFromUtc(utcTime) - kKnownNewMoonJulianDay;
    double fraction = std::fmod(daysSinceKnownNewMoon / kSynodicMonthDays, 1.0);
    if (fraction < 0.0) {
        fraction += 1.0;
    }
    return fraction;
}

[[nodiscard]] double moonIlluminationPercent(const double lunarCycleFraction) noexcept
{
    return std::clamp((1.0 - std::cos(core::MathConstants::kTwoPi * lunarCycleFraction)) * 50.0, 0.0, 100.0);
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
    return compute(ephemerisEngine, requestFromContext(context, ephemerisEngine), sunBodyIndex, moonBodyIndex);
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
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

    const core::SkyContext& context = request.context;
    if (!context.observer.isValid()) {
        return conditions;
    }

    const auto sunState = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(sunBodyIndex));
    const auto moonState = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(moonBodyIndex));
    if (!sunState.has_value() || !sunState->horizontal.isFinite() || !moonState.has_value()
        || !moonState->horizontal.isFinite()) {
        return conditions;
    }

    const ObservationEventCalculator eventCalculator;
    const auto sunHorizon = eventCalculator.compute(ephemerisEngine, request, sunBodyIndex, kSunriseSunsetAltitudeDeg);
    const auto civil = eventCalculator.compute(ephemerisEngine, request, sunBodyIndex, kCivilTwilightAltitudeDeg);
    const auto nautical = eventCalculator.compute(ephemerisEngine, request, sunBodyIndex, kNauticalTwilightAltitudeDeg);
    const auto astronomical =
        eventCalculator.compute(ephemerisEngine, request, sunBodyIndex, kAstronomicalTwilightAltitudeDeg);
    const auto moonHorizon = eventCalculator.compute(ephemerisEngine, request, moonBodyIndex);

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
