#include "NightConditionsCalculator.hpp"
#include "MoonPhaseCalculator.hpp"
#include "ObservationEventCalculator.hpp"
#include "Types.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <cstdint>

namespace skygate::ephemeris {
namespace {

constexpr double kSunriseSunsetAltitudeDeg = -0.833;
constexpr double kCivilTwilightAltitudeDeg = -6.0;
constexpr double kNauticalTwilightAltitudeDeg = -12.0;
constexpr double kAstronomicalTwilightAltitudeDeg = -18.0;

[[nodiscard]] ObservationEvent unavailableEvent() noexcept
{
    return ObservationEvent{.status = ObservationEventStatus::Unresolved};
}

[[nodiscard]] ObservationEventCalculator::SearchMode
observationSearchMode(const NightConditionsCalculator::EventSearchMode mode) noexcept
{
    return mode == NightConditionsCalculator::EventSearchMode::Verified
               ? ObservationEventCalculator::SearchMode::Direct
               : ObservationEventCalculator::SearchMode::GuidedApproximate;
}

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg,
    const NightConditionsCalculator::EventSearchMode eventSearchMode
)
{
    const EphemerisRequest eventRequest = ephemerisRequestForPrecisionPolicy(
        request,
        eventSearchMode == NightConditionsCalculator::EventSearchMode::Verified
            ? EphemerisPrecisionPolicy::NightConditionsVerified
            : EphemerisPrecisionPolicy::NightConditionsApproximate
    );
    return eventCalculator.compute(
        ephemerisEngine, eventRequest, bodyIndex, body, crossingAltitudeDeg, observationSearchMode(eventSearchMode)
    );
}

}  // namespace

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const CelestialBody* sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody* moonBody,
    const EventSearchMode eventSearchMode
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
        eventCalculator, ephemerisEngine, request, moonBodyIndex, moonBody, 0.0, eventSearchMode
    );

    const MoonPhaseCalculator moonPhaseCalculator;
    const MoonPhase moonPhase = moonPhaseCalculator.compute(request.epoch);

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
    conditions.moonIlluminationPercent = moonPhase.illuminationPercent;
    conditions.moonPhaseName = moonPhase.phaseName;
    return conditions;
}

}  // namespace skygate::ephemeris
