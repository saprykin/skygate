#include "EphemerisComputationPolicy.hpp"
#include "BaseCelestialBody.hpp"
#include "EphemerisCorrectionFlags.hpp"
#include "IEphemerisEngine.hpp"

namespace skygate::ephemeris {

EphemerisRequest EphemerisComputationPolicy::correctionFlagsFor(
    const IEphemerisEngine& engine, EphemerisRequest request, const EphemerisPrecisionPolicy policy
) noexcept
{
    if (!engine.traits().supportsSceneCorrectionsPolicy) {
        return request;
    }

    switch (policy) {
    case EphemerisPrecisionPolicy::SceneRender: {
        EphemerisCorrectionFlags corrections = EphemerisCorrectionFlags::precessionNutation()
                                               | EphemerisCorrectionFlags::earthOrientation()
                                               | EphemerisCorrectionFlags::diurnalParallax();
        if (request.options.enableAtmosphericRefraction()
            && EphemerisCorrectionFlags::has(
                request.options.correctionFlags(), EphemerisCorrectionFlags::atmosphericRefraction()
            )) {
            corrections |= EphemerisCorrectionFlags::atmosphericRefraction();
        }
        request.options.setCorrectionFlags(corrections);
        return request;
    }
    case EphemerisPrecisionPolicy::Trail: {
        // A trail is one body sampled along time, so each altitude term it
        // applies also shapes the drawn path. The refraction model ends at the
        // lower edge of its altitude band, where its correction drops to zero,
        // and a trail that crosses that edge gains a step of that size. The
        // rendered body keeps its refracted place; the trail stays geometric.
        request.options.setCorrectionFlags(
            EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::earthOrientation()
            | EphemerisCorrectionFlags::diurnalParallax()
        );
        return request;
    }
    case EphemerisPrecisionPolicy::SelectionDetail:
    case EphemerisPrecisionPolicy::EventSearch:
    case EphemerisPrecisionPolicy::NightConditionsApproximate:
    case EphemerisPrecisionPolicy::NightConditionsVerified:
        return request;
    }

    return request;
}

bool EphemerisComputationPolicy::shouldRecomputeInspectorDetail(
    const IEphemerisEngine& engine, const EphemerisRequest& request
) noexcept
{
    (void)request;
    return engine.traits().supportsInspectorDetailRecompute;
}

ObservationEventCalculator::SearchMode EphemerisComputationPolicy::inspectorEventSearchMode(
    const IEphemerisEngine& engine, const EphemerisRequest& request, const BaseCelestialBody* body
) noexcept
{
    (void)request;
    if (engine.traits().supportsGuidedEventSearch && engine.traits().trustsGuidedSearchResult && body != nullptr
        && !body->hasFixedEquatorial()) {
        return ObservationEventCalculator::SearchMode::GuidedApproximate;
    }

    return ObservationEventCalculator::SearchMode::Guided;
}

bool EphemerisComputationPolicy::trailSamplingIsAdaptive(
    const IEphemerisEngine& engine, const EphemerisRequest& request
) noexcept
{
    (void)request;
    return engine.traits().prefersAdaptiveTrailSampling;
}

bool EphemerisComputationPolicy::trailUsesGuidance(
    const IEphemerisEngine& engine, const EphemerisRequest& request, const BaseCelestialBody* body
) noexcept
{
    return trailSamplingIsAdaptive(engine, request) && engine.traits().allowsTrailGuidanceApproximation
           && body != nullptr;
}

bool EphemerisComputationPolicy::trailAllowsGuidanceApproximation(const IEphemerisEngine& engine) noexcept
{
    return engine.traits().allowsTrailGuidanceApproximation;
}

bool EphemerisComputationPolicy::liveRecomputeThrottleApplies(const IEphemerisEngine& engine) noexcept
{
    return engine.traits().recommendsLiveRecomputeThrottling;
}

}  // namespace skygate::ephemeris
