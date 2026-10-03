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
    case EphemerisPrecisionPolicy::SceneRender:
    case EphemerisPrecisionPolicy::Trail: {
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
    if (engine.traits().supportsGuidedApproximateEventSearch && body != nullptr
        && !body->fixedEquatorialValue().has_value()) {
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
    return trailSamplingIsAdaptive(engine, request) && body != nullptr;
}

bool EphemerisComputationPolicy::liveRecomputeThrottleApplies(const IEphemerisEngine& engine) noexcept
{
    return engine.traits().recommendsLiveRecomputeThrottling;
}

}  // namespace skygate::ephemeris
