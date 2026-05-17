#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/ephemeris/EphemerisPrecisionPolicy.hpp"
#include "skygate/ephemeris/EphemerisRequestFactory.hpp"
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

[[nodiscard]] ObservationEvent unavailableEvent() noexcept
{
    return ObservationEvent{.status = ObservationEventStatus::Unresolved};
}

[[nodiscard]] EphemerisRequest
requestFromContext(const core::SkyContext& context, const IEphemerisEngine& ephemerisEngine) noexcept
{
    return EphemerisRequestFactory::fromContext(context, ephemerisEngine.options());
}

[[nodiscard]] double normalizedLunarCycleFraction(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = normalizedAstronomicalEpoch(epoch);
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double daysSinceKnownNewMoon = julianDay - kKnownNewMoonJulianDay;
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

[[nodiscard]] ObservationEventSearchMode observationSearchMode(const NightConditionsEventSearchMode mode) noexcept
{
    return mode == NightConditionsEventSearchMode::Verified ? ObservationEventSearchMode::Direct
                                                            : ObservationEventSearchMode::Guided;
}

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg,
    const NightConditionsEventSearchMode eventSearchMode
)
{
    const EphemerisRequest eventRequest = ephemerisRequestForPrecisionPolicy(
        request,
        eventSearchMode == NightConditionsEventSearchMode::Verified
            ? EphemerisPrecisionPolicy::NightConditionsVerified
            : EphemerisPrecisionPolicy::NightConditionsApproximate
    );
    if (body != nullptr) {
        return eventCalculator.compute(
            ephemerisEngine, eventRequest, bodyIndex, *body, crossingAltitudeDeg, observationSearchMode(eventSearchMode)
        );
    }

    return eventCalculator.compute(ephemerisEngine, eventRequest, bodyIndex, crossingAltitudeDeg);
}

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const NightConditionsEventSearchMode eventSearchMode
)
{
    return computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, bodyIndex, body, 0.0, eventSearchMode
    );
}

}  // namespace

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t sunBodyIndex,
    const std::uint32_t moonBodyIndex
) const
{
    return compute(
        ephemerisEngine,
        requestFromContext(context, ephemerisEngine),
        sunBodyIndex,
        nullptr,
        moonBodyIndex,
        nullptr,
        NightConditionsEventSearchMode::Approximate
    );
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t sunBodyIndex,
    const CelestialBody& sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody& moonBody
) const
{
    return compute(
        ephemerisEngine,
        requestFromContext(context, ephemerisEngine),
        sunBodyIndex,
        &sunBody,
        moonBodyIndex,
        &moonBody,
        NightConditionsEventSearchMode::Approximate
    );
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const std::uint32_t moonBodyIndex
) const
{
    return compute(
        ephemerisEngine,
        request,
        sunBodyIndex,
        nullptr,
        moonBodyIndex,
        nullptr,
        NightConditionsEventSearchMode::Approximate
    );
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const CelestialBody& sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody& moonBody
) const
{
    return compute(
        ephemerisEngine,
        request,
        sunBodyIndex,
        &sunBody,
        moonBodyIndex,
        &moonBody,
        NightConditionsEventSearchMode::Approximate
    );
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const CelestialBody& sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody& moonBody,
    const NightConditionsEventSearchMode eventSearchMode
) const
{
    return compute(ephemerisEngine, request, sunBodyIndex, &sunBody, moonBodyIndex, &moonBody, eventSearchMode);
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const CelestialBody* sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody* moonBody,
    const NightConditionsEventSearchMode eventSearchMode
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
    const auto sunHorizon = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kSunriseSunsetAltitudeDeg, eventSearchMode
    );
    const auto civil = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kCivilTwilightAltitudeDeg, eventSearchMode
    );
    const auto nautical = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kNauticalTwilightAltitudeDeg, eventSearchMode
    );
    const auto astronomical = computeEventSummaryForNightConditions(
        eventCalculator,
        ephemerisEngine,
        request,
        sunBodyIndex,
        sunBody,
        kAstronomicalTwilightAltitudeDeg,
        eventSearchMode
    );
    const auto moonHorizon = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, moonBodyIndex, moonBody, eventSearchMode
    );

    const double lunarCycleFraction = normalizedLunarCycleFraction(request.epoch);
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
