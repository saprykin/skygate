#include "EpochScaleResolver.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/ITimeScaleService.hpp"

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

    const std::optional<skygate::core::AstronomicalEpoch> resolved = resolveConversion(
        metadata,
        epoch,
        skygate::core::TimeScale::Tdb,
        timeScaleService.get(),
        failurePolicy,
        ConversionMerge::KernelEpoch,
        nullptr
    );
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    return resolved->normalized();
}

std::optional<skygate::core::AstronomicalEpoch> EpochScaleResolver::resolveInScale(
    EphemerisEngineQueryResult& metadata,
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    const skygate::ephemeris::ITimeScaleService* timeScaleService,
    const EphemerisMetadataFailurePolicy failurePolicy,
    std::optional<TimeScaleConversionResult>* const conversionCache,
    const SameScaleConversion sameScaleConversion
) noexcept
{
    if (epoch.timeScale == targetScale && sameScaleConversion == SameScaleConversion::Skip) {
        return epoch.normalized();
    }

    return resolveConversion(
        metadata,
        epoch,
        targetScale,
        timeScaleService,
        failurePolicy,
        ConversionMerge::StatusAndWarnings,
        conversionCache
    );
}

std::optional<skygate::core::AstronomicalEpoch> EpochScaleResolver::resolveConversion(
    EphemerisEngineQueryResult& metadata,
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::core::TimeScale targetScale,
    const skygate::ephemeris::ITimeScaleService* const timeScaleService,
    const EphemerisMetadataFailurePolicy failurePolicy,
    const ConversionMerge merge,
    std::optional<TimeScaleConversionResult>* const conversionCache
) noexcept
{
    if (timeScaleService == nullptr) {
        EphemerisMetadataMerger::markTimeScaleUnavailable(metadata, failurePolicy);
        return std::nullopt;
    }

    std::optional<TimeScaleConversionResult> localConversion;
    std::optional<TimeScaleConversionResult>* const conversionSlot =
        conversionCache != nullptr ? conversionCache : &localConversion;
    if (!conversionSlot->has_value()) {
        *conversionSlot = timeScaleService->convert(epoch, targetScale);
    }

    const TimeScaleConversionResult& conversion = **conversionSlot;
    if (merge == ConversionMerge::KernelEpoch) {
        EphemerisMetadataMerger::mergeKernelEpochTimeScale(metadata, conversion);
    } else {
        EphemerisMetadataMerger::mergeTimeScale(metadata, conversion, failurePolicy);
    }

    if (!conversion.isSuccess()) {
        return std::nullopt;
    }

    return conversion.epoch;
}

}  // namespace skygate::ephemeris::highprecision
