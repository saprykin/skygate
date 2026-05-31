#include "EphemerisMetadataMerger.hpp"

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
    if (conversion.hasWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EarthOrientationDataMissing)
        || conversion.hasWarning(TimeScaleConversionWarningCode::DeltaTUnavailable)) {
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    }
    if (conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable)
        || conversion.hasWarning(TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData)) {
        metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    }
}

void mergeEarthOrientationWarningCodes(
    EphemerisEngineQueryResult& metadata, const EarthOrientationSample& sample
) noexcept
{
    if (sample.hasWarning(EarthOrientationSampleWarningCode::MissingData)) {
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::EpochOutsideRange)) {
        metadata.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    }
}

}  // namespace

void EphemerisMetadataMerger::merge(
    EphemerisEngineQueryResult& target,
    const EphemerisEngineQueryResult& source,
    const EphemerisMetadataMergeOptions options
) noexcept
{
    mergeStatus(target, source, options.statusPolicy);
    target.warningCodeMask |= source.warningCodeMask;

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
    if (conversion.status == TimeScaleConversionStatus::Failed) {
        if (failurePolicy == EphemerisMetadataFailurePolicy::MarkFailed) {
            metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        } else {
            markDegraded(metadata);
        }
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        mergeTimeScaleWarningCodes(metadata, conversion);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Degraded) {
        addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
        mergeTimeScaleWarningCodes(metadata, conversion);
    }
}

void EphemerisMetadataMerger::mergeEarthOrientation(
    EphemerisEngineQueryResult& metadata, const EarthOrientationSample& sample
) noexcept
{
    if (sample.status == EarthOrientationSampleStatus::Failed) {
        metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        mergeEarthOrientationWarningCodes(metadata, sample);
        metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return;
    }
    if (sample.status == EarthOrientationSampleStatus::Degraded) {
        addDegradedWarning(metadata, EphemerisEngineWarning::Code::AccuracyDegraded);
        mergeEarthOrientationWarningCodes(metadata, sample);
    }
}

}  // namespace skygate::ephemeris::highprecision
