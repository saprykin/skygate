#pragma once

#include "engine/EarthOrientationSampler.hpp"
#include "EphemerisMetadataFailurePolicy.hpp"
#include "EphemerisMetadataMergeOptions.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"

#include <optional>

namespace skygate::ephemeris {
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

class ICalcephKernel;

class EphemerisMetadataMerger final {
public:
    EphemerisMetadataMerger() = delete;

    // Reusable conversion and sample slots for repeated preparation calls.
    struct EarthOrientationCache {
        std::optional<TimeScaleConversionResult> utcConversion;
        std::optional<EarthOrientationSampler::Sample> sample;
    };

    static void merge(
        EphemerisEngineQueryResult& target,
        const EphemerisEngineQueryResult& source,
        EphemerisMetadataMergeOptions options = {}
    ) noexcept;

    static void markCorrectionUnavailable(
        EphemerisEngineQueryResult& metadata, EphemerisCorrectionFlags unavailableCorrection
    ) noexcept;

    static void
    markCorrectionFailed(EphemerisEngineQueryResult& metadata, EphemerisCorrectionFlags unavailableCorrection) noexcept;

    static void
    markCorrectionApplied(EphemerisEngineQueryResult& metadata, EphemerisCorrectionFlags appliedCorrection) noexcept;

    static void mergeTimeScale(
        EphemerisEngineQueryResult& metadata,
        const TimeScaleConversionResult& conversion,
        EphemerisMetadataFailurePolicy failurePolicy = EphemerisMetadataFailurePolicy::MarkDegraded
    ) noexcept;

    static void
    mergeEarthOrientation(EphemerisEngineQueryResult& metadata, const EarthOrientationSampler::Sample& sample) noexcept;

    static void mergeWarnings(EphemerisEngineQueryResult& target, const EphemerisEngineQueryResult& source) noexcept;

    static void mergeKernelDiagnostics(EphemerisEngineQueryResult& metadata, const ICalcephKernel& kernel) noexcept;

    // Converts 'epoch' to UTC, samples Earth orientation, and merges the
    // time-scale conversion and the sample into 'metadata'. Returns the sample
    // when both steps succeed; otherwise marks the Earth-orientation
    // correction unavailable. 'cache' reuses a conversion and sample across
    // repeated calls.
    [[nodiscard]] static std::optional<EarthOrientationSampler::Sample> prepareEarthOrientation(
        const skygate::core::AstronomicalEpoch& epoch,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider,
        EphemerisEngineQueryResult& metadata,
        EphemerisMetadataFailurePolicy timeScaleFailurePolicy,
        const EarthOrientationSampler::Options& sampleOptions,
        EarthOrientationCache* cache = nullptr
    );
};

}  // namespace skygate::ephemeris::highprecision
