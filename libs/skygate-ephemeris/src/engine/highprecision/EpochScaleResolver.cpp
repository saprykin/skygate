#include "EpochScaleResolver.hpp"
#include "EphemerisTimeScaleMetadataMerger.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"

namespace skygate::ephemeris::highprecision {

std::optional<skygate::core::AstronomicalEpoch> EpochScaleResolver::resolve(
    EphemerisEngineQueryResult& metadata,
    const skygate::core::AstronomicalEpoch& epoch,
    const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
    const EphemerisMetadataFailurePolicy failurePolicy
) noexcept
{
    if (epoch.timeScale == skygate::core::TimeScale::Tdb) {
        return epoch.normalized();
    }

    if (timeScaleService == nullptr) {
        metadata.status = failurePolicy == EphemerisMetadataFailurePolicy::MarkFailed
                              ? EphemerisEngineQueryStatus::Type::Failed
                              : EphemerisEngineQueryStatus::Type::Degraded;
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return std::nullopt;
    }

    const TimeScaleConversionResult conversion = timeScaleService->convert(epoch, skygate::core::TimeScale::Tdb);
    EphemerisTimeScaleMetadataMerger::mergeKernelEpochTimeScaleMetadata(metadata, conversion);
    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch.normalized();
}

}  // namespace skygate::ephemeris::highprecision
