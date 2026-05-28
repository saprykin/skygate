#include "ObservationEventCalculator.hpp"
#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequestFactory.hpp"
#include "UtcTimeCodec.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "factory/EphemerisEngineFactory.hpp"

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
constexpr auto kRefinementTolerance = std::chrono::seconds(1);
constexpr auto kGuidedRefinementTolerance = std::chrono::seconds(60);
constexpr double kAltitudeClassificationToleranceDeg = 1e-9;

struct AltitudeSample final {
    core::UtcTimePoint utcTime;
    double altitudeDeg = std::numeric_limits<double>::quiet_NaN();
};

enum class SampleRole : std::uint8_t {
    Direct,
    Guidance
};

using SearchMode = ObservationEventCalculator::SearchMode;

[[nodiscard]] bool
isRiseBracket(const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg < crossingAltitudeDeg && next.altitudeDeg >= crossingAltitudeDeg;
}

[[nodiscard]] bool
isSetBracket(const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg) noexcept
{
    return previous.altitudeDeg > crossingAltitudeDeg && next.altitudeDeg <= crossingAltitudeDeg;
}

[[nodiscard]] bool isCrossingBracket(
    const AltitudeSample& previous, const AltitudeSample& next, bool rising, double crossingAltitudeDeg
) noexcept
{
    return rising ? isRiseBracket(previous, next, crossingAltitudeDeg)
                  : isSetBracket(previous, next, crossingAltitudeDeg);
}

[[nodiscard]] core::UtcTimePoint interpolatedCrossingTime(
    const AltitudeSample& previous, const AltitudeSample& next, double crossingAltitudeDeg
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

[[nodiscard]] ObservationEventStatus unavailableCrossingStatus(
    const std::vector<AltitudeSample>& samples, std::optional<ObservationEventStatus> provenFixedStatus
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

class EventSearch final {
public:
    EventSearch(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t bodyIndex,
        const BaseCelestialBody* body,
        double crossingAltitudeDeg
    ) noexcept
        : m_ephemerisEngine(ephemerisEngine), m_request(request), m_bodyIndex(bodyIndex), m_body(body),
          m_crossingAltitudeDeg(crossingAltitudeDeg)
    {
    }

    [[nodiscard]] std::vector<AltitudeSample>
    sampleAltitudes(const IEphemerisEngine& engine, const EphemerisRequest& request, std::uint32_t bodyIndex);

    [[nodiscard]] ObservationEvent findCrossing(
        const std::vector<AltitudeSample>& samples,
        SampleRole sampleRole,
        bool trustGuidance,
        bool rising,
        std::optional<ObservationEventStatus> provenFixedStatus
    );

    [[nodiscard]] ObservationEvent findCulmination(const std::vector<AltitudeSample>& samples, bool trustGuidance);

    [[nodiscard]] std::optional<ObservationEventStatus> fixedHorizonStatus() const noexcept;

    [[nodiscard]] static ObservationEventSummary invalidSummary() noexcept
    {
        return ObservationEventSummary{
            .nextRise = {.status = ObservationEventStatus::InvalidInput},
            .nextSet = {.status = ObservationEventStatus::InvalidInput},
            .culmination = {.status = ObservationEventStatus::InvalidInput}
        };
    }

private:
    [[nodiscard]] std::optional<double> altitudeAt(const core::UtcTimePoint& utcTime);

    [[nodiscard]] std::optional<std::array<AltitudeSample, 2>>
    originalAltitudeBracket(const AltitudeSample& previous, const AltitudeSample& next);

    [[nodiscard]] core::UtcTimePoint refinedCrossingTime(
        const AltitudeSample& previous,
        const AltitudeSample& next,
        bool rising,
        core::UtcTimePoint::duration refinementTolerance
    );

    [[nodiscard]] AltitudeSample refinedMaximum(const core::UtcTimePoint& startUtc, const core::UtcTimePoint& endUtc);

    const IEphemerisEngine& m_ephemerisEngine;
    const EphemerisRequest& m_request;
    std::uint32_t m_bodyIndex;
    const BaseCelestialBody* m_body;
    double m_crossingAltitudeDeg;
};

std::optional<double> EventSearch::altitudeAt(const core::UtcTimePoint& utcTime)
{
    const auto request = EphemerisRequestFactory::atUtcTime(m_request, utcTime);
    const auto state = m_ephemerisEngine.computeBodyState(request, static_cast<std::size_t>(m_bodyIndex));
    if (!state.has_value() || !state->horizontal.isFinite()) {
        return std::nullopt;
    }
    return state->horizontal.altitudeDeg;
}

std::vector<AltitudeSample> EventSearch::sampleAltitudes(
    const IEphemerisEngine& engine, const EphemerisRequest& request, const std::uint32_t bodyIndex
)
{
    std::vector<AltitudeSample> samples;
    samples.reserve((kSearchHorizonSeconds / kSampleStepSeconds) + 1);

    for (int offsetSeconds = 0; offsetSeconds <= kSearchHorizonSeconds; offsetSeconds += kSampleStepSeconds) {
        const core::UtcTimePoint utcTime = request.context.utcTime + std::chrono::seconds(offsetSeconds);
        const auto sampleRequest = EphemerisRequestFactory::atUtcTime(request, utcTime);
        const auto state = engine.computeBodyState(sampleRequest, static_cast<std::size_t>(bodyIndex));
        if (!state.has_value() || !state->horizontal.isFinite()) {
            continue;
        }
        samples.push_back(AltitudeSample{.utcTime = utcTime, .altitudeDeg = state->horizontal.altitudeDeg});
    }

    return samples;
}

std::optional<ObservationEventStatus> EventSearch::fixedHorizonStatus() const noexcept
{
    if (m_body == nullptr || !m_body->fixedEquatorialValue().has_value()) {
        return std::nullopt;
    }

    const core::EquatorialCoordinate& equatorial = *m_body->fixedEquatorialValue();
    if (!equatorial.isFinite()) {
        return std::nullopt;
    }

    const double maxAltitudeDeg = 90.0 - std::abs(m_request.context.observer.latitudeDeg - equatorial.declinationDeg);
    const double minAltitudeDeg = std::abs(m_request.context.observer.latitudeDeg + equatorial.declinationDeg) - 90.0;

    if (minAltitudeDeg >= m_crossingAltitudeDeg - kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysAbove;
    }
    if (maxAltitudeDeg <= m_crossingAltitudeDeg + kAltitudeClassificationToleranceDeg) {
        return ObservationEventStatus::AlwaysBelow;
    }

    return std::nullopt;
}

std::optional<std::array<AltitudeSample, 2>>
EventSearch::originalAltitudeBracket(const AltitudeSample& previous, const AltitudeSample& next)
{
    const auto previousAltitude = altitudeAt(previous.utcTime);
    const auto nextAltitude = altitudeAt(next.utcTime);
    if (!previousAltitude.has_value() || !nextAltitude.has_value()) {
        return std::nullopt;
    }

    return std::array<AltitudeSample, 2>{
        AltitudeSample{.utcTime = previous.utcTime, .altitudeDeg = *previousAltitude},
        AltitudeSample{.utcTime = next.utcTime, .altitudeDeg = *nextAltitude},
    };
}

core::UtcTimePoint EventSearch::refinedCrossingTime(
    const AltitudeSample& previous,
    const AltitudeSample& next,
    const bool rising,
    const core::UtcTimePoint::duration refinementTolerance
)
{
    core::UtcTimePoint low = previous.utcTime;
    core::UtcTimePoint high = next.utcTime;

    while (high - low > refinementTolerance) {
        const auto midpointOffset = (high - low) / 2;
        const core::UtcTimePoint midpoint = low + midpointOffset;
        const auto midpointAltitude = altitudeAt(midpoint);
        if (!midpointAltitude.has_value()) {
            break;
        }

        if (rising) {
            if (*midpointAltitude >= m_crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        } else {
            if (*midpointAltitude <= m_crossingAltitudeDeg) {
                high = midpoint;
            } else {
                low = midpoint;
            }
        }
    }

    return high;
}

ObservationEvent EventSearch::findCrossing(
    const std::vector<AltitudeSample>& samples,
    const SampleRole sampleRole,
    const bool trustGuidance,
    const bool rising,
    const std::optional<ObservationEventStatus> provenFixedStatus
)
{
    bool rejectedGuidanceBracket = false;

    for (std::size_t index = 1; index < samples.size(); ++index) {
        AltitudeSample previous = samples[index - 1U];
        AltitudeSample next = samples[index];
        if (!isCrossingBracket(previous, next, rising, m_crossingAltitudeDeg)) {
            continue;
        }

        if (sampleRole == SampleRole::Guidance && !trustGuidance) {
            const auto originalBracket = originalAltitudeBracket(previous, next);
            if (!originalBracket.has_value()
                || !isCrossingBracket((*originalBracket)[0], (*originalBracket)[1], rising, m_crossingAltitudeDeg)) {
                rejectedGuidanceBracket = true;
                continue;
            }
            previous = (*originalBracket)[0];
            next = (*originalBracket)[1];
        }

        const core::UtcTimePoint crossingUtcTime =
            trustGuidance ? interpolatedCrossingTime(previous, next, m_crossingAltitudeDeg)
                          : refinedCrossingTime(
                                previous,
                                next,
                                rising,
                                sampleRole == SampleRole::Guidance ? kGuidedRefinementTolerance : kRefinementTolerance
                            );

        return ObservationEvent{
            .status = ObservationEventStatus::Available,
            .utcTime = crossingUtcTime,
            .altitudeDeg = m_crossingAltitudeDeg
        };
    }

    if (rejectedGuidanceBracket) {
        return ObservationEvent{.status = ObservationEventStatus::Unresolved};
    }

    return ObservationEvent{.status = unavailableCrossingStatus(samples, provenFixedStatus)};
}

AltitudeSample EventSearch::refinedMaximum(const core::UtcTimePoint& startUtc, const core::UtcTimePoint& endUtc)
{
    auto lowSeconds = core::UtcTimeCodec::toEpochSecondsFloor(startUtc);
    auto highSeconds = core::UtcTimeCodec::toEpochSecondsFloor(endUtc);

    while (highSeconds - lowSeconds > 3) {
        const auto spanSeconds = highSeconds - lowSeconds;
        const auto firstSeconds = lowSeconds + spanSeconds / 3;
        const auto secondSeconds = highSeconds - spanSeconds / 3;
        const core::UtcTimePoint firstUtc = core::UtcTimeCodec::fromEpochSeconds(firstSeconds);
        const core::UtcTimePoint secondUtc = core::UtcTimeCodec::fromEpochSeconds(secondSeconds);
        const auto firstAltitude = altitudeAt(firstUtc);
        const auto secondAltitude = altitudeAt(secondUtc);
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
        const auto altitude = altitudeAt(utcTime);
        if (altitude.has_value() && *altitude > best.altitudeDeg) {
            best.utcTime = utcTime;
            best.altitudeDeg = *altitude;
        }
    }

    return best;
}

ObservationEvent EventSearch::findCulmination(const std::vector<AltitudeSample>& samples, const bool trustGuidance)
{
    for (std::size_t index = 1; index + 1U < samples.size(); ++index) {
        const AltitudeSample& previous = samples[index - 1U];
        const AltitudeSample& current = samples[index];
        const AltitudeSample& next = samples[index + 1U];

        if (current.altitudeDeg < previous.altitudeDeg || current.altitudeDeg < next.altitudeDeg) {
            continue;
        }

        const AltitudeSample maximum = trustGuidance ? current : refinedMaximum(previous.utcTime, next.utcTime);
        if (!std::isfinite(maximum.altitudeDeg)) {
            break;
        }

        return ObservationEvent{
            .status = ObservationEventStatus::Available, .utcTime = maximum.utcTime, .altitudeDeg = maximum.altitudeDeg
        };
    }

    return ObservationEvent{
        .status = samples.empty() ? ObservationEventStatus::Unresolved : ObservationEventStatus::NoEventInSearchWindow,
        .utcTime = std::nullopt,
        .altitudeDeg = std::nullopt
    };
}

[[nodiscard]] bool
shouldUseGuidanceEngine(const BaseCelestialBody* body, const EphemerisRequest& request, SearchMode searchMode) noexcept
{
    return (searchMode == SearchMode::Guided || searchMode == SearchMode::GuidedApproximate) && body != nullptr
           && request.options.engineKind() == EphemerisEngineKind::Type::HighPrecision;
}

[[nodiscard]] bool
isTrustingGuidanceModel(const BaseCelestialBody* body, const EphemerisRequest& request, SearchMode searchMode) noexcept
{
    return searchMode == SearchMode::GuidedApproximate
           || (body != nullptr && body->fixedEquatorialValue().has_value()
               && request.options.correctionFlags() != EphemerisCorrectionFlags::noCorrections());
}

[[nodiscard]] bool shouldFallBackToDirect(
    SearchMode searchMode,
    const BaseCelestialBody* body,
    const ObservationEvent& nextRise,
    const ObservationEvent& nextSet
) noexcept
{
    if (searchMode == SearchMode::GuidedApproximate) {
        return false;
    }

    const bool fixedBody = body != nullptr && body->fixedEquatorialValue().has_value();
    const bool unresolved =
        nextRise.status == ObservationEventStatus::Unresolved || nextSet.status == ObservationEventStatus::Unresolved;
    const bool nonFixedMiss = !fixedBody
                              && (nextRise.status == ObservationEventStatus::NoEventInSearchWindow
                                  || nextSet.status == ObservationEventStatus::NoEventInSearchWindow);
    return unresolved || nonFixedMiss;
}

[[nodiscard]] std::optional<std::vector<AltitudeSample>>
sampleGuidanceAltitudes(EventSearch& search, const BaseCelestialBody& body, const EphemerisRequest& request)
{
    const std::array<const BaseCelestialBody*, 1> bodies{&body};
    auto factoryResult =
        EphemerisEngineFactory::create(CelestialBodyCatalog(std::span<const BaseCelestialBody* const>{bodies}));
    std::unique_ptr<IEphemerisEngine> guidanceEngine = std::move(factoryResult.engine);
    if (guidanceEngine == nullptr) {
        return std::nullopt;
    }

    EphemerisRequest guidanceRequest = request;
    guidanceRequest.options = guidanceEngine->options();
    guidanceRequest.options.setEngineKind(guidanceEngine->kind());

    auto samples = search.sampleAltitudes(*guidanceEngine, guidanceRequest, 0U);
    if (samples.empty()) {
        return std::nullopt;
    }

    return std::move(samples);
}

}  // namespace

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BaseCelestialBody* body,
    const double crossingAltitudeDeg,
    const SearchMode searchMode
) const
{
    const core::ObservationContext& context = request.context;
    if (!context.observer.isValid() || !std::isfinite(crossingAltitudeDeg)) {
        return EventSearch::invalidSummary();
    }

    EventSearch search(ephemerisEngine, request, bodyIndex, body, crossingAltitudeDeg);

    SampleRole sampleRole = SampleRole::Direct;
    std::vector<AltitudeSample> samples;

    if (shouldUseGuidanceEngine(body, request, searchMode)) {
        if (auto guidanceSamples = sampleGuidanceAltitudes(search, *body, request); guidanceSamples.has_value()) {
            samples = std::move(*guidanceSamples);
            sampleRole = SampleRole::Guidance;
        }
    }

    if (samples.empty()) {
        samples = search.sampleAltitudes(ephemerisEngine, request, bodyIndex);
        sampleRole = SampleRole::Direct;
    }
    if (samples.empty()) {
        return ObservationEventSummary{};
    }

    const auto provenFixedStatus = search.fixedHorizonStatus();
    const bool trustGuidance = sampleRole == SampleRole::Guidance && isTrustingGuidanceModel(body, request, searchMode);

    ObservationEvent nextRise = search.findCrossing(samples, sampleRole, trustGuidance, true, provenFixedStatus);
    ObservationEvent nextSet = search.findCrossing(samples, sampleRole, trustGuidance, false, provenFixedStatus);
    ObservationEvent culmination = search.findCulmination(samples, trustGuidance);

    if (sampleRole == SampleRole::Guidance && shouldFallBackToDirect(searchMode, body, nextRise, nextSet)) {
        samples = search.sampleAltitudes(ephemerisEngine, request, bodyIndex);
        if (samples.empty()) {
            return ObservationEventSummary{};
        }

        sampleRole = SampleRole::Direct;
        nextRise = search.findCrossing(samples, sampleRole, false, true, provenFixedStatus);
        nextSet = search.findCrossing(samples, sampleRole, false, false, provenFixedStatus);
        culmination = search.findCulmination(samples, false);
    }

    return ObservationEventSummary{.nextRise = nextRise, .nextSet = nextSet, .culmination = culmination};
}

}  // namespace skygate::ephemeris
