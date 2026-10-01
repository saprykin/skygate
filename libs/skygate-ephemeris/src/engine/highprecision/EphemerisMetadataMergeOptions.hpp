#pragma once

#include "EphemerisMetadataStatusMergePolicy.hpp"

namespace skygate::ephemeris::highprecision {

struct EphemerisMetadataMergeOptions {
    EphemerisMetadataStatusMergePolicy statusPolicy = EphemerisMetadataStatusMergePolicy::FullResultStatus;
    bool mergeCorrections = true;
    bool mergeProvenance = true;
    bool mergeValidityRange = true;
    bool mergeAngularUncertainty = true;
};

}  // namespace skygate::ephemeris::highprecision
