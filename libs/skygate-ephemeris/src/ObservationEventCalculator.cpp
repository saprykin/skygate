#include "ObservationEventCalculator.hpp"
#include "CelestialBodyCatalog.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationEventSearch.hpp"
#include "ObservationEventSearchPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

using AltitudeSample = ObservationEventSearch::AltitudeSample;
using SampleRole = ObservationEventSearch::SampleRole;

[[nodiscard]] std::optional<std::vector<AltitudeSample>> sampleGuidanceAltitudes(
    ObservationEventSearch& search,
    const BaseCelestialBody& body,
    const EphemerisRequest& request,
    const IEphemerisGuidanceStrategy& guidanceStrategy
)
{
    const std::array<const BaseCelestialBody*, 1> bodies{&body};
    std::unique_ptr<IEphemerisEngine> guidanceEngine =
        guidanceStrategy.createGuidanceEngine(CelestialBodyCatalog(std::span<const BaseCelestialBody* const>{bodies}));
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

ObservationEventCalculator::ObservationEventCalculator(std::shared_ptr<IEphemerisGuidanceStrategy> guidanceStrategy)
    : m_guidanceStrategy(std::move(guidanceStrategy))
{
}

ObservationEventSummary ObservationEventCalculator::compute(
    const IEphemerisEngine& ephemerisEngine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BaseCelestialBody* body,
    const double crossingAltitudeDeg,
    const SearchMode searchMode
) const
{
    const skygate::core::ObservationContext& context = request.context;
    if (!context.observer.isValid() || !std::isfinite(crossingAltitudeDeg)) {
        return ObservationEventSearch::invalidSummary();
    }

    ObservationEventSearch search(ephemerisEngine, request, bodyIndex, body, crossingAltitudeDeg);

    SampleRole sampleRole = SampleRole::Direct;
    std::vector<AltitudeSample> samples;

    if (m_guidanceStrategy != nullptr && ObservationEventSearchPolicy::shouldUseGuidanceEngine(ephemerisEngine, body)) {
        if (auto guidanceSamples = sampleGuidanceAltitudes(search, *body, request, *m_guidanceStrategy);
            guidanceSamples.has_value()) {
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
    const bool trustGuidance =
        sampleRole == SampleRole::Guidance
        && ObservationEventSearchPolicy::isTrustingGuidanceModel(ephemerisEngine, body, request, searchMode);

    ObservationEvent nextRise = search.findCrossing(samples, sampleRole, trustGuidance, true, provenFixedStatus);
    ObservationEvent nextSet = search.findCrossing(samples, sampleRole, trustGuidance, false, provenFixedStatus);
    ObservationEvent culmination = search.findCulmination(samples, trustGuidance);

    if (sampleRole == SampleRole::Guidance
        && ObservationEventSearchPolicy::shouldFallBackToDirect(searchMode, body, trustGuidance, nextRise, nextSet)) {
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
