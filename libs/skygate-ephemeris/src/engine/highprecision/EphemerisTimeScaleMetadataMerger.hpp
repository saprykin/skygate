#pragma once

namespace skygate::ephemeris {

struct EphemerisEngineQueryResult;
struct TimeScaleConversionResult;

namespace highprecision {

/// Merges kernel-epoch time-scale conversion diagnostics into an engine query
/// result.
///
/// A degraded conversion that carries only the TDB approximation warning is
/// considered transparent and is not merged; every other successful or failed
/// conversion outcome is merged through the common metadata merger.
class EphemerisTimeScaleMetadataMerger final {
public:
    EphemerisTimeScaleMetadataMerger() = delete;

    static void mergeKernelEpochTimeScaleMetadata(
        EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
    ) noexcept;
};

}  // namespace highprecision
}  // namespace skygate::ephemeris
