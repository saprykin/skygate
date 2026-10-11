#include "EphemerisMetadataMerger.hpp"
#include "ICalcephKernel.hpp"
#include "engine/ITimeScaleService.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace skygate::ephemeris::highprecision {
namespace {

void mergeStatus(
    EphemerisEngineQueryResult& target,
    const EphemerisEngineQueryResult& source,
    const EphemerisMetadataStatusMergePolicy policy
) noexcept
{
    if (source.status == EphemerisEngineQueryStatus::Type::Failed) {
        target.status = EphemerisEngineQueryStatus::Type::Failed;
        return;
    }
    if (policy == EphemerisMetadataStatusMergePolicy::FullResultStatus
        && source.status == EphemerisEngineQueryStatus::Type::OutOfRange) {
        target.status = EphemerisEngineQueryStatus::Type::OutOfRange;
        return;
    }
    if (policy == EphemerisMetadataStatusMergePolicy::FullResultStatus
        && source.status == EphemerisEngineQueryStatus::Type::Unsupported) {
        target.status = EphemerisEngineQueryStatus::Type::Unsupported;
        return;
    }
    if (source.status == EphemerisEngineQueryStatus::Type::Degraded
        && target.status == EphemerisEngineQueryStatus::Type::Valid) {
        target.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
}

void markDegraded(EphemerisEngineQueryResult& metadata) noexcept
{
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
}

void addDegradedWarning(EphemerisEngineQueryResult& metadata, const EphemerisEngineWarning::Code warningCode) noexcept
{
    markDegraded(metadata);
    metadata.addWarning(warningCode);
}

void mergeTimeScaleWarningCodes(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    const auto addWarning = [&metadata, &conversion](
                                const TimeScaleConversionWarningCode::Type sourceCode,
                                const std::optional<EphemerisEngineWarning::Code> aggregateCode = std::nullopt
                            ) noexcept {
        if (!conversion.hasWarning(sourceCode)) {
            return;
        }

        metadata.addWarning(EphemerisEngineWarning::fromTimeScaleWarning(sourceCode), conversion.diagnosticText);
        if (aggregateCode.has_value()) {
            metadata.addWarning(*aggregateCode);
        }
    };

    addWarning(
        TimeScaleConversionWarningCode::Type::LeapSecondTableMissing,
        EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableStale);
    addWarning(
        TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable, EphemerisEngineWarning::Code::DataOutOfRange
    );
    addWarning(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied);
    addWarning(TimeScaleConversionWarningCode::Type::UnsupportedConversion);
    addWarning(TimeScaleConversionWarningCode::Type::InvalidInput);
    addWarning(TimeScaleConversionWarningCode::Type::TdbApproximationApplied);
    addWarning(
        TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing,
        EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataStale);
    addWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted);
    addWarning(
        TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData,
        EphemerisEngineWarning::Code::DataOutOfRange
    );
    addWarning(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied);
    addWarning(
        TimeScaleConversionWarningCode::Type::DeltaTUnavailable, EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated);
}

void mergeEarthOrientationWarningCodes(
    EphemerisEngineQueryResult& metadata, const EarthOrientationSampler::Sample& sample
) noexcept
{
    const auto addWarning = [&metadata, &sample](
                                const EarthOrientationSampler::Sample::WarningCode sourceCode,
                                const std::optional<EphemerisEngineWarning::Code> aggregateCode = std::nullopt
                            ) noexcept {
        if (!sample.hasWarning(sourceCode)) {
            return;
        }

        metadata.addWarning(EarthOrientationSampler::Sample::engineWarningCode(sourceCode), sample.diagnosticText);
        if (aggregateCode.has_value()) {
            metadata.addWarning(*aggregateCode);
        }
    };

    addWarning(EarthOrientationSampler::Sample::WarningCode::StaleData);
    addWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData);
    addWarning(
        EarthOrientationSampler::Sample::WarningCode::MissingData,
        EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(
        EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange, EphemerisEngineWarning::Code::DataOutOfRange
    );
    addWarning(EarthOrientationSampler::Sample::WarningCode::InvalidInput);
    addWarning(EarthOrientationSampler::Sample::WarningCode::EstimatedData);
}

}  // namespace

void EphemerisMetadataMerger::merge(
    EphemerisEngineQueryResult& target,
    const EphemerisEngineQueryResult& source,
    const EphemerisMetadataMergeOptions options
) noexcept
{
    mergeStatus(target, source, options.statusPolicy);
    mergeWarnings(target, source);

    if (options.mergeCorrections) {
        target.appliedCorrections |= source.appliedCorrections;
        target.unavailableCorrections |= source.unavailableCorrections;
    }
    if (options.mergeProvenance && target.dataSourceProvenance.empty()) {
        target.dataSourceProvenance = source.dataSourceProvenance;
    }
    if (options.mergeValidityRange && !target.effectiveDataValidityRange.has_value()) {
        target.effectiveDataValidityRange = source.effectiveDataValidityRange;
    }
    if (options.mergeAngularUncertainty && !target.estimatedAngularUncertaintyArcsec.has_value()) {
        target.estimatedAngularUncertaintyArcsec = source.estimatedAngularUncertaintyArcsec;
    }
}

void EphemerisMetadataMerger::markCorrectionUnavailable(
    EphemerisEngineQueryResult& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    markDegraded(metadata);
    metadata.addUnavailableCorrection(unavailableCorrection);
}

void EphemerisMetadataMerger::markCorrectionFailed(
    EphemerisEngineQueryResult& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    markCorrectionUnavailable(metadata, unavailableCorrection);
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid
        || metadata.status == EphemerisEngineQueryStatus::Type::Degraded) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    }
    metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
}

void EphemerisMetadataMerger::markCorrectionApplied(
    EphemerisEngineQueryResult& metadata, const EphemerisCorrectionFlags appliedCorrection
) noexcept
{
    metadata.appliedCorrections |= appliedCorrection;
}

void EphemerisMetadataMerger::mergeTimeScale(
    EphemerisEngineQueryResult& metadata,
    const TimeScaleConversionResult& conversion,
    const EphemerisMetadataFailurePolicy failurePolicy
) noexcept
{
    if (conversion.status == TimeScaleConversionStatus::Type::Failed) {
        if (failurePolicy == EphemerisMetadataFailurePolicy::MarkFailed) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        } else {
            markDegraded(metadata);
        }
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        mergeTimeScaleWarningCodes(metadata, conversion);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Type::Degraded) {
        addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
        mergeTimeScaleWarningCodes(metadata, conversion);
    }
}

void EphemerisMetadataMerger::mergeEarthOrientation(
    EphemerisEngineQueryResult& metadata, const EarthOrientationSampler::Sample& sample
) noexcept
{
    if (sample.status == EarthOrientationSampler::Sample::Status::Failed) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        mergeEarthOrientationWarningCodes(metadata, sample);
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return;
    }
    if (sample.status == EarthOrientationSampler::Sample::Status::Degraded) {
        addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
        mergeEarthOrientationWarningCodes(metadata, sample);
    }
}

void EphemerisMetadataMerger::mergeWarnings(
    EphemerisEngineQueryResult& target, const EphemerisEngineQueryResult& source
) noexcept
{
    target.warningCodeMask |= source.warningCodeMask;
    target.warningDetails.insert(
        target.warningDetails.end(), source.warningDetails.begin(), source.warningDetails.end()
    );
}

void EphemerisMetadataMerger::mergeKernelDiagnostics(
    EphemerisEngineQueryResult& metadata, const ICalcephKernel& kernel
) noexcept
{
    for (const std::string& diagnostic : kernel.diagnostics()) {
        if (diagnostic.empty()) {
            continue;
        }

        const bool alreadyPresent = std::any_of(
            metadata.warningDetails.begin(),
            metadata.warningDetails.end(),
            [&diagnostic](const EphemerisEngineWarning::Detail& detail) noexcept { return detail.text == diagnostic; }
        );
        if (!alreadyPresent) {
            metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed, diagnostic);
        }
    }
}

std::optional<EarthOrientationSampler::Sample> EphemerisMetadataMerger::prepareEarthOrientation(
    const skygate::core::AstronomicalEpoch& epoch,
    const skygate::ephemeris::ITimeScaleService* timeScaleService,
    const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider,
    EphemerisEngineQueryResult& metadata,
    const EphemerisMetadataFailurePolicy timeScaleFailurePolicy,
    const EarthOrientationSampler::Options& sampleOptions,
    EarthOrientationCache* cache
)
{
    if (!epoch.isFinite()) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        return std::nullopt;
    }

    std::optional<TimeScaleConversionResult> localConversion;
    std::optional<TimeScaleConversionResult>* conversionSlot =
        cache != nullptr ? &cache->utcConversion : &localConversion;
    if (timeScaleService == nullptr) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return std::nullopt;
    } else if (!conversionSlot->has_value()) {
        *conversionSlot = timeScaleService->convert(epoch, skygate::core::TimeScale::Utc);
    }

    const TimeScaleConversionResult& conversion = **conversionSlot;
    mergeTimeScale(metadata, conversion, timeScaleFailurePolicy);
    if (!conversion.isSuccess()) {
        markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::earthOrientation());
        return std::nullopt;
    }

    std::optional<EarthOrientationSampler::Sample> localSample;
    std::optional<EarthOrientationSampler::Sample>& sampleSlot = cache != nullptr ? cache->sample : localSample;
    if (!sampleSlot.has_value()) {
        sampleSlot = EarthOrientationSampler::sample(earthOrientationProvider, conversion.epoch, sampleOptions);
    }

    mergeEarthOrientation(metadata, *sampleSlot);
    if (!sampleSlot->isSuccess()) {
        markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::earthOrientation());
        return std::nullopt;
    }

    return sampleSlot;
}

}  // namespace skygate::ephemeris::highprecision
