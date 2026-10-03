#pragma once

namespace skygate::ephemeris {

// Sampling and approximation preferences are intentionally separate.
//
// prefersAdaptiveTrailSampling selects the adaptive direct-sampling method.
// allowsTrailGuidanceApproximation grants permission to replace that direct
// sampling with a cheaper approximation (the guidance strategy or the
// fixed-equatorial shortcut). An engine may request adaptive sampling without
// approving approximation, or may approve approximation for a non-adaptive
// trail (which consumers still resolve through their own policy).
//
// supportsGuidedEventSearch enables guidance-backed event search, while
// trustsGuidedSearchResult grants permission to return the approximate guided
// answer directly instead of refining it against the primary engine.
struct EphemerisEngineTraits final {
    bool supportsSceneCorrectionsPolicy = false;
    bool supportsInspectorDetailRecompute = false;
    bool supportsGuidedEventSearch = false;
    bool trustsGuidedSearchResult = false;
    bool prefersAdaptiveTrailSampling = false;
    bool allowsTrailGuidanceApproximation = false;
    bool recommendsLiveRecomputeThrottling = false;

    [[nodiscard]] static constexpr EphemerisEngineTraits noTraits() noexcept
    {
        return {};
    }

    [[nodiscard]] static constexpr EphemerisEngineTraits highPrecisionEngine() noexcept
    {
        EphemerisEngineTraits traits;
        traits.supportsSceneCorrectionsPolicy = true;
        traits.supportsInspectorDetailRecompute = true;
        traits.supportsGuidedEventSearch = true;
        traits.trustsGuidedSearchResult = true;
        traits.prefersAdaptiveTrailSampling = true;
        traits.allowsTrailGuidanceApproximation = true;
        traits.recommendsLiveRecomputeThrottling = true;
        return traits;
    }
};

}  // namespace skygate::ephemeris
