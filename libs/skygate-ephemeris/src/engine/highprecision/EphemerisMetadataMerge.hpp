#pragma once

#include "engine/highprecision/EarthOrientationProvider.hpp"
#include "engine/highprecision/TimeScaleService.hpp"
#include "Types.hpp"

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
        EphemerisResultMetadata& target,
        const EphemerisResultMetadata& source,
        EphemerisMetadataMergeOptions options = {}
    ) noexcept;

    static void markCorrectionUnavailable(
        EphemerisResultMetadata& metadata, EphemerisCorrectionFlags unavailableCorrection
    ) noexcept;

    static void mergeTimeScale(
        EphemerisResultMetadata& metadata,
        const TimeScaleConversionResult& conversion,
        EphemerisMetadataFailurePolicy failurePolicy = EphemerisMetadataFailurePolicy::MarkDegraded
    ) noexcept;

    static void mergeEarthOrientation(EphemerisResultMetadata& metadata, const EarthOrientationSample& sample) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
