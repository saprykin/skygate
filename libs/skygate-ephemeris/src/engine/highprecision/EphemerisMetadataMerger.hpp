#pragma once

#include "EarthOrientationProvider.hpp"
#include "TimeScaleService.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"

namespace skygate::ephemeris::highprecision {

enum class EphemerisMetadataStatusMergePolicy {
    FullResultStatus,
    DegradedAndFailedOnly
};

enum class EphemerisMetadataFailurePolicy {
    MarkDegraded,
    MarkFailed
};

struct EphemerisMetadataMergeOptions {
    EphemerisMetadataStatusMergePolicy statusPolicy = EphemerisMetadataStatusMergePolicy::FullResultStatus;
    bool mergeCorrections = true;
    bool mergeProvenance = true;
    bool mergeValidityRange = true;
    bool mergeAngularUncertainty = true;
};

class EphemerisMetadataMerger final {
public:
    EphemerisMetadataMerger() = delete;

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
    mergeEarthOrientation(EphemerisEngineQueryResult& metadata, const EarthOrientationSample& sample) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
