#include "EphemerisTimeScaleMetadataMerger.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/TimeScaleConversionResult.hpp"

namespace skygate::ephemeris::highprecision {

void EphemerisTimeScaleMetadataMerger::mergeKernelEpochTimeScaleMetadata(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    const TimeScaleConversionResult::WarningMask tdbApproximationMask =
        TimeScaleConversionResult::WarningMask::fromBits(
            TimeScaleConversionResult::WarningMask::bit(TimeScaleConversionWarningCode::Type::TdbApproximationApplied)
        );
    const bool onlyTdbApproximation = !conversion.warningCodeMask.without(tdbApproximationMask).hasAny();
    if (conversion.status == TimeScaleConversionStatus::Type::Degraded && onlyTdbApproximation) {
        return;
    }

    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);
}

}  // namespace skygate::ephemeris::highprecision
