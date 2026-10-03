#include "EphemerisTimeScaleMetadataMerger.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/TimeScaleConversionResult.hpp"

#include <cstdint>

namespace skygate::ephemeris::highprecision {

void EphemerisTimeScaleMetadataMerger::mergeKernelEpochTimeScaleMetadata(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    constexpr std::uint32_t kTdbApproximationWarning =
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::TdbApproximationApplied);
    if (conversion.status == TimeScaleConversionStatus::Degraded
        && (conversion.warningCodeMask & ~kTdbApproximationWarning) == 0U) {
        return;
    }

    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);
}

}  // namespace skygate::ephemeris::highprecision
