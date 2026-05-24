#include "ObservationEventCalculator.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "EphemerisRequestFactory.hpp"
#include "IEphemerisEngine.hpp"
#include "Types.hpp"
#include "UtcTimeCodec.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr int kSampleStepSeconds = 10 * 60;
constexpr int kSearchHorizonSeconds = 72 * 60 * 60;
constexpr int kRefinementToleranceSeconds = 1;
constexpr int kGuidedRefinementToleranceSeconds = 60;
constexpr double kAltitudeClassificationToleranceDeg = 1e-9;

struct AltitudeSample final {
    core::UtcTimePoint utcTime;
    double altitudeDeg = std::numeric_limits<double>::quiet_NaN();
};

enum class SampleRole : std::uint8_t {
    Direct,
    Guidance
};

struct AltitudeSamples final {
    std::vector<AltitudeSample> values;
    SampleRole role = SampleRole::Direct;
    bool trustGuidanceModel = false;
};

[[nodiscard]] core::UtcTimePoint addSeconds(const core::UtcTimePoint& utcTime, const int seconds) noexcept
{
    return utcTime + std::chrono::seconds(seconds);
}

[[nodiscard]] EphemerisRequest
requestFromContext(const core::SkyContext& context, const IEphemerisEngine& ephemerisEngine) noexcept
{
    return EphemerisRequestFactory::fromContext(context, ephemerisEngine.options());
}

[[nodiscard]] std::optional<double> altitudeAt(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& baseRequest,
    const std::uint32_t bodyIndex,
    const core::UtcTimePoint& utcTime
)
{
    const auto request = EphemerisRequestFactory::atUtcTime(baseRequest, utcTime);
    const auto state = ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(bodyIndex));
    if (!state.has_value() || !state->horizontal.isFinite()) {
        return std::nullopt;
    }
    return state->horizontal.altitudeDeg;
}

[[nodiscard]] std::vector<AltitudeSample>
sampleAltitudes(const IEphemerisEngine& ephemerisEngine, const EphemerisRequest& request, const std::uint32_t bodyIndex)
{
    std::vector<AltitudeSample> samples;
    samples.reserve((kSearchHorizonSeconds / kSampleStepSeconds) + 1);

    for (int offsetSeconds = 0; offsetSeconds <= kSearchHorizonSeconds; offsetSeconds += kSampleStepSeconds) {
        const core::UtcTimePoint utcTime = addSeconds(request.context.utcTime, offsetSeconds);
        const auto altitude = altitudeAt(ephemerisEngine, request, bodyIndex, utcTime);
        if (!altitude.has_value()) {
            continue;
        }

        samples.push_back(AltitudeSample{.utcTime = utcTime, .altitudeDeg = *altitude});
    }

    return samples;
}

[[nodiscard]] bool shouldUseGuidanceEngine(
    const EphemerisRequest& request, const CelestialBody* body, const ObservationEventSearchMode searchMode
) noexcept
{
    return (searchMode == ObservationEventSearchMode::Guided
            || searchMode == ObservationEventSearchMode::GuidedApproximate)
           && body != nullptr && request.options.engineKind == EphemerisEngineKind::HighPrecision;
}

[[nodiscard]] std::optional<AltitudeSamples> sampleGuidanceAltitudes(
    const EphemerisRequest& request, const CelestialBody& body, const ObservationEventSearchMode searchMode
)
{
    const std::array<CelestialBody, 1> bodies{body};
    auto guidanceEngineResult =
        EphemerisEngineFactory::create(std::span<const CelestialBody>{bodies.data(), bodies.size()});
    std::unique_ptr<IEphemerisEngine> guidanceEngine = std::move(guidanceEngineResult.engine);
    if (guidanceEngine == nullptr) {
        return std::nullopt;
    }

    EphemerisRequest guidanceRequest = request;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.engineKind = guidanceEngine->kind();

    return AltitudeSamples{
        .values = sampleAltitudes(*guidanceEngine, guidanceRequest, 0U),
        .role = SampleRole::Guidance,
        .trustGuidanceModel = searchMode == ObservationEventSearchMode::GuidedApproximate
                              || (body.fixedEquatorial.has_value()
                                  && request.options.correctionFlags != EphemerisCorrectionFlags::NoCorrections)
    };
}

[[nodiscard]] AltitudeSamples sampleEventSearchAltitudes(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const ObservationEventSearchMode searchMode
)
{
    if (shouldUseGuidanceEngine(request, body, searchMode)) {
        if (std::optional<AltitudeSamples> guidanceSamples = sampleGuidanceAltitudes(request, *body, searchMode);
            guidanceSamples.has_value() && !guidanceSamples->values.empty()) {
            return std::move(*guidanceSamples);
        }
    }

    return AltitudeSamples{
        .values = sampleAltitudes(ephemerisEngine, request, bodyIndex),
        .role = SampleRole::Direct,
        .trustGuidanceModel = false,
    };
}

[[nodiscard]] std::optional<ObservationEventStatus> fixedHorizonStatus(
    const CelestialBody* body, const core::GeoLocation& observer, const double crossingAltitudeDeg
) noexcept
{
    if (body == nullptr || !body->fixedEquatorial.has_value()) {
        return std::nullopt;
    }

    const core::EquatorialCoordinate& equatorial = *body->fixedEquatorial;
    if (!equatorial.isFinite()) {
        return std::nullopt;
    }

    const double maxAltitudeDeg = 90.0 - std::abs(observer.latitudeDeg - equatorial.declinationDeg);
    const double minAltitudeDeg = std::abs(observer.latitudeDeg + equatorial.declinationDeg) - 90.0;

    if (minAltitudeDeg >= crossingAltitudeDeg - kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysAbove;
    }
    if (maxAltitudeDeg <= crossingAltitudeDeg + kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysBelow;
    }

    return std::nullopt;
}

[[nodiscard]] ObservationEventStatus unavailableCrossingStatus(
    const std::vector<AltitudeSample>& samples, const std::optional<ObservationEventStatus> provenFixedStatus
) noexcept
{
    if (samples.empty()) {
        return ObservationEventStatus::Unresolved;
    }
    if (provenFixedStatus.has_value()) {
        return *provenFixedStatus;
    }

    return ObservationEventStatus::NoEventInSearchWindow;
}

[[nodiscard]] bool
isRiseBracket(const AltitudeSample& previous, const AltitudeSample& next, const double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg < crossingAltitudeDeg && next.altitudeDeg >= crossingAltitudeDeg;
}

[[nodiscard]] bool
isSetBracket(const AltitudeSample& previous, const AltitudeSample& next, const double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg > crossingAltitudeDeg && next.altitudeDeg <= crossingAltitudeDeg;
}

[[nodiscard]] bool isCrossingBracket(
    const AltitudeSample& previous, const AltitudeSample& next, const bool rising, const double crossingAltitudeDeg
) noexcept
{
    return rising ? isRiseBracket(previous, next, crossingAltitudeDeg)
                  : isSetBracket(previous, next, crossingAltitudeDeg);
}

[[nodiscard]] std::optional<std::array<AltitudeSample, 2>> originalAltitudeBracket(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const AltitudeSample& previous,
    const AltitudeSample& next
)
{
    const auto previousAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, previous.utcTime);
    const auto nextAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, next.utcTime);
    if (!previousAltitude.has_value() || !nextAltitude.has_value()) {
        return std::nullopt;
    }

    return std::array<AltitudeSample, 2>{
        AltitudeSample{.utcTime = previous.utcTime, .altitudeDeg = *previousAltitude},
        AltitudeSample{.utcTime = next.utcTime, .altitudeDeg = *nextAltitude},
    };
}

[[nodiscard]] core::UtcTimePoint refinedCrossingTime(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const AltitudeSample& previous,
    const AltitudeSample& next,
    const bool rising,
    const double crossingAltitudeDeg,
    const int refinementToleranceSeconds
)
{
    core::UtcTimePoint low = previous.utcTime;
    core::UtcTimePoint high = next.utcTime;

    while ((high - low).count() > refinementToleranceSeconds) {
        const auto midpointOffset = (high - low) / 2;
        const core::UtcTimePoint midpoint = low + midpointOffset;
        const auto midpointAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, midpoint);
        if (!midpointAltitude.has_value()) {
            break;
        }

        if (rising) {
            if (*midpointAltitude >= crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        } else {
            if (*midpointAltitude <= crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        }
    }

    return high;
}

[[nodiscard]] core::UtcTimePoint interpolatedCrossingTime(
    const AltitudeSample& previous, const AltitudeSample& next, const double crossingAltitudeDeg
) noexcept
{
    const double altitudeSpanDeg = next.altitudeDeg - previous.altitudeDeg;
    if (altitudeSpanDeg == 0.0 || !std::isfinite(altitudeSpanDeg)) {
        return next.utcTime;
    }

    const double fraction = std::clamp((crossingAltitudeDeg - previous.altitudeDeg) / altitudeSpanDeg, 0.0, 1.0);
    const auto span = next.utcTime - previous.utcTime;
    return previous.utcTime
           + std::chrono::duration_cast<core::UtcTimePoint::duration>(
               std::chrono::duration<double>(static_cast<double>(span.count()) * fraction)
           );
}

[[nodiscard]] ObservationEvent findCrossing(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const std::vector<AltitudeSample>& samples,
    const SampleRole sampleRole,
    const bool trustGuidanceModel,
    const bool rising,
    const std::optional<ObservationEventStatus> provenFixedStatus,
    const double crossingAltitudeDeg
)
{
    bool rejectedGuidanceBracket = false;
    for (std::size_t index = 1; index < samples.size(); ++index) {
        AltitudeSample previous = samples[index - 1U];
        AltitudeSample next = samples[index];
        if (!isCrossingBracket(previous, next, rising, crossingAltitudeDeg)) {
            continue;
        }

        if (sampleRole == SampleRole::Guidance && !trustGuidanceModel) {
            const auto originalBracket = originalAltitudeBracket(ephemerisEngine, request, bodyIndex, previous, next);
            if (!originalBracket.has_value()
                || !isCrossingBracket((*originalBracket)[0], (*originalBracket)[1], rising, crossingAltitudeDeg)) {
                rejectedGuidanceBracket = true;
                continue;
            }

            previous = (*originalBracket)[0];
            next = (*originalBracket)[1];
        }

        return ObservationEvent{
            .status = ObservationEventStatus::Available,
            .utcTime = trustGuidanceModel ? interpolatedCrossingTime(previous, next, crossingAltitudeDeg)
                                          : refinedCrossingTime(
                                                ephemerisEngine,
                                                request,
                                                bodyIndex,
                                                previous,
                                                next,
                                                rising,
                                                crossingAltitudeDeg,
                                                sampleRole == SampleRole::Guidance ? kGuidedRefinementToleranceSeconds
                                                                                   : kRefinementToleranceSeconds
                                            )
        };
    }

    if (rejectedGuidanceBracket) {
        return ObservationEvent{.status = ObservationEventStatus::Unresolved, .utcTime = std::nullopt};
    }

    return ObservationEvent{.status = unavailableCrossingStatus(samples, provenFixedStatus), .utcTime = std::nullopt};
}

[[nodiscard]] AltitudeSample refinedMaximum(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const core::UtcTimePoint& startUtc,
    const core::UtcTimePoint& endUtc
)
{
    auto lowSeconds = core::UtcTimeCodec::toEpochSecondsFloor(startUtc);
    auto highSeconds = core::UtcTimeCodec::toEpochSecondsFloor(endUtc);

    while (highSeconds - lowSeconds > 3) {
        const auto spanSeconds = highSeconds - lowSeconds;
        const auto firstSeconds = lowSeconds + spanSeconds / 3;
        const auto secondSeconds = highSeconds - spanSeconds / 3;
        const core::UtcTimePoint firstUtc = core::UtcTimeCodec::fromEpochSeconds(firstSeconds);
        const core::UtcTimePoint secondUtc = core::UtcTimeCodec::fromEpochSeconds(secondSeconds);
        const auto firstAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, firstUtc);
        const auto secondAltitude = altitudeAt(ephemerisEngine, request, bodyIndex, secondUtc);
        if (!firstAltitude.has_value() || !secondAltitude.has_value()) {
            break;
        }

        if (*firstAltitude < *secondAltitude) {
            lowSeconds = firstSeconds;
        } else {
            highSeconds = secondSeconds;
        }
    }

    AltitudeSample best{.utcTime = startUtc, .altitudeDeg = -std::numeric_limits<double>::infinity()};
    for (auto seconds = lowSeconds; seconds <= highSeconds; ++seconds) {
        const core::UtcTimePoint utcTime = core::UtcTimeCodec::fromEpochSeconds(seconds);
        const auto altitude = altitudeAt(ephemerisEngine, request, bodyIndex, utcTime);
        if (altitude.has_value() && *altitude > best.altitudeDeg) {
            best.utcTime = utcTime;
            best.altitudeDeg = *altitude;
        }
    }

    return best;
}

[[nodiscard]] ObservationCulmination findCulmination(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const std::vector<AltitudeSample>& samples,
    const bool trustGuidanceModel
)
{
    for (std::size_t index = 1; index + 1U < samples.size(); ++index) {
        const AltitudeSample& previous = samples[index - 1U];
        const AltitudeSample& current = samples[index];
        const AltitudeSample& next = samples[index + 1U];

        if (current.altitudeDeg < previous.altitudeDeg || current.altitudeDeg < next.altitudeDeg) {
            continue;
        }

        const AltitudeSample maximum =
            trustGuidanceModel ? current
                               : refinedMaximum(ephemerisEngine, request, bodyIndex, previous.utcTime, next.utcTime);
        if (!std::isfinite(maximum.altitudeDeg)) {
            break;
        }

        return ObservationCulmination{
            .status = ObservationEventStatus::Available, .utcTime = maximum.utcTime, .altitudeDeg = maximum.altitudeDeg
        };
    }

    return ObservationCulmination{
        .status = samples.empty() ? ObservationEventStatus::Unresolved : ObservationEventStatus::NoEventInSearchWindow,
        .utcTime = std::nullopt,
        .altitudeDeg = std::nullopt
    };
}

[[nodiscard]] ObservationEventSummary invalidSummary() noexcept
{
    return ObservationEventSummary{
        .nextRise = {.status = ObservationEventStatus::InvalidInput},
        .nextSet = {.status = ObservationEventStatus::InvalidInput},
        .culmination = {.status = ObservationEventStatus::InvalidInput}
    };
}

[[nodiscard]] ObservationEventSummary computeObservationEvents(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody* body,
    const double crossingAltitudeDeg,
    const ObservationEventSearchMode searchMode
)
{
    const core::SkyContext& context = request.context;
    if (!context.observer.isValid() || !std::isfinite(crossingAltitudeDeg)) {
        return invalidSummary();
    }

    auto samples = sampleEventSearchAltitudes(ephemerisEngine, request, bodyIndex, body, searchMode);
    if (samples.values.empty()) {
        return ObservationEventSummary{};
    }

    const auto provenFixedStatus = fixedHorizonStatus(body, context.observer, crossingAltitudeDeg);
    ObservationEvent nextRise = findCrossing(
        ephemerisEngine,
        request,
        bodyIndex,
        samples.values,
        samples.role,
        samples.trustGuidanceModel,
        true,
        provenFixedStatus,
        crossingAltitudeDeg
    );
    ObservationEvent nextSet = findCrossing(
        ephemerisEngine,
        request,
        bodyIndex,
        samples.values,
        samples.role,
        samples.trustGuidanceModel,
        false,
        provenFixedStatus,
        crossingAltitudeDeg
    );
    ObservationCulmination culmination =
        findCulmination(ephemerisEngine, request, bodyIndex, samples.values, samples.trustGuidanceModel);

    const bool approximateGuidance = searchMode == ObservationEventSearchMode::GuidedApproximate;
    if (approximateGuidance && samples.role == SampleRole::Guidance) {
        return ObservationEventSummary{.nextRise = nextRise, .nextSet = nextSet, .culmination = culmination};
    }

    const bool fixedGuidanceBody = body != nullptr && body->fixedEquatorial.has_value();
    const bool nonFixedGuidanceMiss = samples.role == SampleRole::Guidance && !fixedGuidanceBody
                                      && (nextRise.status == ObservationEventStatus::NoEventInSearchWindow
                                          || nextSet.status == ObservationEventStatus::NoEventInSearchWindow);
    if (searchMode == ObservationEventSearchMode::Guided && samples.role == SampleRole::Guidance
        && (nextRise.status == ObservationEventStatus::Unresolved
            || nextSet.status == ObservationEventStatus::Unresolved || nonFixedGuidanceMiss)) {
        samples = AltitudeSamples{
            .values = sampleAltitudes(ephemerisEngine, request, bodyIndex),
            .role = SampleRole::Direct,
            .trustGuidanceModel = false,
        };
        if (samples.values.empty()) {
            return ObservationEventSummary{};
        }

        nextRise = findCrossing(
            ephemerisEngine,
            request,
            bodyIndex,
            samples.values,
            samples.role,
            samples.trustGuidanceModel,
            true,
            provenFixedStatus,
            crossingAltitudeDeg
        );
        nextSet = findCrossing(
            ephemerisEngine,
            request,
            bodyIndex,
            samples.values,
            samples.role,
            samples.trustGuidanceModel,
            false,
            provenFixedStatus,
            crossingAltitudeDeg
        );
        culmination = findCulmination(ephemerisEngine, request, bodyIndex, samples.values, samples.trustGuidanceModel);
    }

    return ObservationEventSummary{.nextRise = nextRise, .nextSet = nextSet, .culmination = culmination};
}

}  // namespace

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine, const core::SkyContext& context, const std::uint32_t bodyIndex
) const
{
    return compute(ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine,
        requestFromContext(context, ephemerisEngine),
        bodyIndex,
        nullptr,
        crossingAltitudeDeg,
        ObservationEventSearchMode::Guided
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const CelestialBody& body
) const
{
    return compute(ephemerisEngine, requestFromContext(context, ephemerisEngine), bodyIndex, body, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const CelestialBody& body,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine,
        requestFromContext(context, ephemerisEngine),
        bodyIndex,
        &body,
        crossingAltitudeDeg,
        ObservationEventSearchMode::Guided
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine, const EphemerisRequest& request, const std::uint32_t bodyIndex
) const
{
    return compute(ephemerisEngine, request, bodyIndex, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine, request, bodyIndex, nullptr, crossingAltitudeDeg, ObservationEventSearchMode::Guided
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody& body
) const
{
    return compute(ephemerisEngine, request, bodyIndex, body, 0.0);
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody& body,
    const double crossingAltitudeDeg
) const
{
    return computeObservationEvents(
        ephemerisEngine, request, bodyIndex, &body, crossingAltitudeDeg, ObservationEventSearchMode::Guided
    );
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const CelestialBody& body,
    const double crossingAltitudeDeg,
    const ObservationEventSearchMode searchMode
) const
{
    return computeObservationEvents(ephemerisEngine, request, bodyIndex, &body, crossingAltitudeDeg, searchMode);
}

}  // namespace skygate::ephemeris
