#include "NightConditionsCalculator.hpp"
#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "MoonPhaseCalculator.hpp"
#include "ObservationEventCalculator.hpp"
#include "SolarAltitudeThresholds.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BaseCelestialBody* body,
    const double crossingAltitudeDeg
)
{
    const EphemerisRequest eventRequest = EphemerisRequest::fromPrecisionPolicy(
        request, EphemerisPrecisionPolicy::NightConditionsApproximate, ephemerisEngine
    );
    return eventCalculator.compute(
        ephemerisEngine,
        eventRequest,
        bodyIndex,
        body,
        crossingAltitudeDeg,
        ObservationEventCalculator::SearchMode::GuidedApproximate
    );
}

}  // namespace

NightConditionsCalculator::NightConditionsCalculator(std::shared_ptr<IEphemerisGuidanceStrategy> guidanceStrategy)
    : m_guidanceStrategy(std::move(guidanceStrategy))
{
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const BaseCelestialBody* sunBody,
    const std::uint32_t moonBodyIndex,
    const BaseCelestialBody* moonBody
) const
{
    NightConditions conditions;
    const skygate::core::ObservationContext& context = request.context;
    if (!context.observer.isValid()) {
        return conditions;
    }

    const auto sunState = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(sunBodyIndex));
    const auto moonState = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(moonBodyIndex));
    if (!sunState.has_value() || !sunState->horizontal.isFinite() || !moonState.has_value()
        || !moonState->horizontal.isFinite()) {
        return conditions;
    }

    const ObservationEventCalculator eventCalculator(m_guidanceStrategy);
    const auto sunHorizon = computeEventSummaryForNightConditions(
        eventCalculator,
        ephemerisEngine,
        request,
        sunBodyIndex,
        sunBody,
        SolarAltitudeThresholds::kSunriseSunsetAltitudeDeg
    );
    const auto civil = computeEventSummaryForNightConditions(
        eventCalculator,
        ephemerisEngine,
        request,
        sunBodyIndex,
        sunBody,
        SolarAltitudeThresholds::kCivilTwilightAltitudeDeg
    );
    const auto nautical = computeEventSummaryForNightConditions(
        eventCalculator,
        ephemerisEngine,
        request,
        sunBodyIndex,
        sunBody,
        SolarAltitudeThresholds::kNauticalTwilightAltitudeDeg
    );
    const auto astronomical = computeEventSummaryForNightConditions(
        eventCalculator,
        ephemerisEngine,
        request,
        sunBodyIndex,
        sunBody,
        SolarAltitudeThresholds::kAstronomicalTwilightAltitudeDeg
    );
    const auto moonHorizon =
        computeEventSummaryForNightConditions(eventCalculator, ephemerisEngine, request, moonBodyIndex, moonBody, 0.0);

    const MoonPhaseCalculator moonPhaseCalculator;
    const MoonPhase moonPhase = moonPhaseCalculator.compute(request.epoch);

    conditions.valid = true;
    conditions.sunrise = sunHorizon.nextRise;
    conditions.sunset = sunHorizon.nextSet;
    conditions.civilDusk = civil.nextSet;
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
