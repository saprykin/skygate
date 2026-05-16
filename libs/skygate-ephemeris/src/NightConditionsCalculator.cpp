#include "skygate/ephemeris/NightConditionsCalculator.hpp"

#include "skygate/core/math/MathConstants.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

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

[[nodiscard]] ObservationEventSummary computeEventSummary(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg
)
{
    if (body != nullptr) {
        return eventCalculator.compute(ephemerisEngine, request, bodyIndex, *body, crossingAltitudeDeg);
    }

    return eventCalculator.compute(ephemerisEngine, request, bodyIndex, crossingAltitudeDeg);
}

[[nodiscard]] ObservationEventSummary computeEventSummary(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body
)
{
    if (body != nullptr) {
        return eventCalculator.compute(ephemerisEngine, request, bodyIndex, *body);
    }

    return eventCalculator.compute(ephemerisEngine, request, bodyIndex);
}

[[nodiscard]] bool shouldUseApproximateEventEngine(const EphemerisRequest& request, const CelestialBody* body) noexcept
{
    return body != nullptr && request.options.engineKind == EphemerisEngineKind::HighPrecision;
}

[[nodiscard]] EphemerisRequest
approximateEventRequest(const EphemerisRequest& request, const IEphemerisEngine& eventEngine) noexcept
{
    EphemerisRequest eventRequest = request;
    eventRequest.options = eventEngine.options();
    eventRequest.options.engineKind = eventEngine.kind();
    return eventRequest;
}

[[nodiscard]] CelestialBody approximateEventBody(CelestialBody body)
{
    if (body.fixedEquatorial.has_value()) {
        body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
        return body;
    }

    if (body.ephemerisSource != CelestialBodyEphemerisSource::Unresolved) {
        return body;
    }

    switch (body.type) {
    case CelestialBodyType::Sun:
        body.ephemerisSource = CelestialBodyEphemerisSource::Sun;
        break;
    case CelestialBodyType::Moon:
        body.ephemerisSource = CelestialBodyEphemerisSource::Moon;
        break;
    case CelestialBodyType::Planet:
        body.ephemerisSource = CelestialBodyEphemerisSource::Planet;
        break;
    case CelestialBodyType::Star:
    case CelestialBodyType::Constellation:
    case CelestialBodyType::DeepSkyObject:
        break;
    }

    return body;
}

[[nodiscard]] std::unique_ptr<IEphemerisEngine> makeApproximateEventEngine(const CelestialBody& body)
{
    const std::array<CelestialBody, 1> bodies{approximateEventBody(body)};
    return createEphemerisEngine(std::span<const CelestialBody>{bodies.data(), bodies.size()});
}

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg
)
{
    if (!shouldUseApproximateEventEngine(request, body)) {
        return computeEventSummary(eventCalculator, ephemerisEngine, request, bodyIndex, body, crossingAltitudeDeg);
    }

    std::unique_ptr<IEphemerisEngine> eventEngine = makeApproximateEventEngine(*body);
    if (eventEngine == nullptr) {
        return computeEventSummary(eventCalculator, ephemerisEngine, request, bodyIndex, body, crossingAltitudeDeg);
    }

    const EphemerisRequest eventRequest = approximateEventRequest(request, *eventEngine);
    return eventCalculator.compute(*eventEngine, eventRequest, 0U, *body, crossingAltitudeDeg);
}

[[nodiscard]] ObservationEventSummary computeEventSummaryForNightConditions(
    const ObservationEventCalculator& eventCalculator,
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body
)
{
    if (!shouldUseApproximateEventEngine(request, body)) {
        return computeEventSummary(eventCalculator, ephemerisEngine, request, bodyIndex, body);
    }

    std::unique_ptr<IEphemerisEngine> eventEngine = makeApproximateEventEngine(*body);
    if (eventEngine == nullptr) {
        return computeEventSummary(eventCalculator, ephemerisEngine, request, bodyIndex, body);
    }

    const EphemerisRequest eventRequest = approximateEventRequest(request, *eventEngine);
    return eventCalculator.compute(*eventEngine, eventRequest, 0U, *body);
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
    const core::SkyContext& context,
    const std::uint32_t sunBodyIndex,
    const CelestialBody& sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody& moonBody
) const
{
    return compute(
        ephemerisEngine, requestFromContext(context, ephemerisEngine), sunBodyIndex, sunBody, moonBodyIndex, moonBody
    );
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const std::uint32_t moonBodyIndex
) const
{
    return compute(ephemerisEngine, request, sunBodyIndex, nullptr, moonBodyIndex, nullptr);
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
    return compute(ephemerisEngine, request, sunBodyIndex, &sunBody, moonBodyIndex, &moonBody);
}

NightConditions NightConditionsCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t sunBodyIndex,
    const CelestialBody* sunBody,
    const std::uint32_t moonBodyIndex,
    const CelestialBody* moonBody
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
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kSunriseSunsetAltitudeDeg
    );
    const auto civil = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kCivilTwilightAltitudeDeg
    );
    const auto nautical = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kNauticalTwilightAltitudeDeg
    );
    const auto astronomical = computeEventSummaryForNightConditions(
        eventCalculator, ephemerisEngine, request, sunBodyIndex, sunBody, kAstronomicalTwilightAltitudeDeg
    );
    const auto moonHorizon =
        computeEventSummaryForNightConditions(eventCalculator, ephemerisEngine, request, moonBodyIndex, moonBody);

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
