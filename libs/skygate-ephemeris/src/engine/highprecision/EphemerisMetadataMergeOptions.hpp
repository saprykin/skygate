#pragma once

#include "EphemerisMetadataStatusMergePolicy.hpp"

namespace skygate::ephemeris::highprecision {

struct EphemerisMetadataMergeOptions {
    EphemerisMetadataStatusMergePolicy statusPolicy = EphemerisMetadataStatusMergePolicy::FullResultStatus;
    bool mergeCorrections = true;
    bool mergeProvenance = true;
    bool mergeValidityRange = true;
    bool mergeAngularUncertainty = true;

    // Keeps the full result status, provenance, validity range, and
    // uncertainty; correction flags stay with the caller that applies them.
    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions withoutCorrections() noexcept
    {
        EphemerisMetadataMergeOptions options;
        options.mergeCorrections = false;
        return options;
    }

    // Keeps degraded or failed status and warnings, the transport metadata a
    // cached or staged frame transform carries. Provenance, validity range,
    // and uncertainty belong to the source query and never travel with it.
    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions statusAndWarningsOnly() noexcept
    {
        EphemerisMetadataMergeOptions options;
        options.statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly;
        options.mergeCorrections = false;
        options.mergeProvenance = false;
        options.mergeValidityRange = false;
        options.mergeAngularUncertainty = false;
        return options;
    }
};

}  // namespace skygate::ephemeris::highprecision
