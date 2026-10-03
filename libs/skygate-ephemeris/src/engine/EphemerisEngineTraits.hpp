#pragma once

namespace skygate::ephemeris {

struct EphemerisEngineTraits final {
    bool supportsSceneCorrectionsPolicy = false;
    bool supportsInspectorDetailRecompute = false;
    bool supportsGuidedApproximateEventSearch = false;
    bool prefersAdaptiveTrailSampling = false;
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
        traits.supportsGuidedApproximateEventSearch = true;
        traits.prefersAdaptiveTrailSampling = true;
        traits.recommendsLiveRecomputeThrottling = true;
        return traits;
    }
};

}  // namespace skygate::ephemeris
