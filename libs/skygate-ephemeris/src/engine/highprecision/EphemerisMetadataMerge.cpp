#include "engine/highprecision/EphemerisMetadataMerge.hpp"

namespace skygate::ephemeris::highprecision {
namespace {

void mergeStatus(
    EphemerisResultMetadata& target,
    const EphemerisResultMetadata& source,
    const EphemerisMetadataStatusMergePolicy policy
) noexcept
{
    if (source.status == EphemerisResultStatus::Failed) {
        target.status = EphemerisResultStatus::Failed;
        return;
    }
    if (policy == EphemerisMetadataStatusMergePolicy::FullResultStatus
        && source.status == EphemerisResultStatus::OutOfRange) {
        target.status = EphemerisResultStatus::OutOfRange;
        return;
    }
    if (policy == EphemerisMetadataStatusMergePolicy::FullResultStatus
        && source.status == EphemerisResultStatus::Unsupported) {
        target.status = EphemerisResultStatus::Unsupported;
        return;
    }
    if (source.status == EphemerisResultStatus::Degraded && target.status == EphemerisResultStatus::Valid) {
        target.status = EphemerisResultStatus::Degraded;
    }
}

}  // namespace

void EphemerisMetadataMerger::merge(
    EphemerisResultMetadata& target, const EphemerisResultMetadata& source, const EphemerisMetadataMergeOptions options
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
    EphemerisResultMetadata& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    if (metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
    }
    metadata.addUnavailableCorrection(unavailableCorrection);
}

void EphemerisMetadataMerger::mergeTimeScale(
    EphemerisResultMetadata& metadata,
    const TimeScaleConversionResult& conversion,
    const EphemerisMetadataFailurePolicy failurePolicy
) noexcept
{
    if (conversion.status == TimeScaleConversionStatus::Failed) {
        if (failurePolicy == EphemerisMetadataFailurePolicy::MarkFailed) {
            metadata.status = EphemerisResultStatus::Failed;
        } else if (metadata.status == EphemerisResultStatus::Valid) {
            metadata.status = EphemerisResultStatus::Degraded;
        }
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return;
    }
    if (conversion.status == TimeScaleConversionStatus::Degraded && metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }
}

void EphemerisMetadataMerger::mergeEarthOrientation(
    EphemerisResultMetadata& metadata, const EarthOrientationSample& sample
) noexcept
{
    if (sample.status == EarthOrientationSampleStatus::Failed) {
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
        return;
    }
    if (sample.status == EarthOrientationSampleStatus::Degraded && metadata.status == EphemerisResultStatus::Valid) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::MissingData)) {
        metadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    }
    if (sample.hasWarning(EarthOrientationSampleWarningCode::EpochOutsideRange)) {
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    }
}

}  // namespace skygate::ephemeris::highprecision
