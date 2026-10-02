#include "EphemerisMetadataMerger.hpp"
#include "ICalcephKernel.hpp"

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

[[nodiscard]] EphemerisEngineWarning::Code warningCodeFor(const TimeScaleConversionWarningCode code) noexcept
{
    switch (code) {
    case TimeScaleConversionWarningCode::LeapSecondTableMissing:
        return EphemerisEngineWarning::Code::LeapSecondTableMissing;
    case TimeScaleConversionWarningCode::LeapSecondTableStale:
        return EphemerisEngineWarning::Code::LeapSecondTableStale;
    case TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable:
        return EphemerisEngineWarning::Code::EpochOutsideLeapSecondTable;
    case TimeScaleConversionWarningCode::LeapSecondFallbackApplied:
        return EphemerisEngineWarning::Code::LeapSecondFallbackApplied;
    case TimeScaleConversionWarningCode::UnsupportedConversion:
        return EphemerisEngineWarning::Code::UnsupportedTimeScaleConversion;
    case TimeScaleConversionWarningCode::InvalidInput:
        return EphemerisEngineWarning::Code::InvalidTimeScaleInput;
    case TimeScaleConversionWarningCode::TdbApproximationApplied:
        return EphemerisEngineWarning::Code::TdbApproximationApplied;
    case TimeScaleConversionWarningCode::EarthOrientationDataMissing:
        return EphemerisEngineWarning::Code::EarthOrientationDataMissing;
    case TimeScaleConversionWarningCode::EarthOrientationDataStale:
        return EphemerisEngineWarning::Code::EarthOrientationDataStale;
    case TimeScaleConversionWarningCode::EarthOrientationDataPredicted:
        return EphemerisEngineWarning::Code::EarthOrientationDataPredicted;
    case TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData:
        return EphemerisEngineWarning::Code::EpochOutsideEarthOrientationData;
    case TimeScaleConversionWarningCode::DeltaTFallbackApplied:
        return EphemerisEngineWarning::Code::DeltaTFallbackApplied;
    case TimeScaleConversionWarningCode::DeltaTUnavailable:
        return EphemerisEngineWarning::Code::DeltaTUnavailable;
    case TimeScaleConversionWarningCode::EarthOrientationDataEstimated:
        return EphemerisEngineWarning::Code::EarthOrientationDataEstimated;
    }

    return EphemerisEngineWarning::Code::AccuracyDegraded;
}

[[nodiscard]] EphemerisEngineWarning::Code
warningCodeFor(const EarthOrientationSampler::Sample::WarningCode code) noexcept
{
    switch (code) {
    case EarthOrientationSampler::Sample::WarningCode::StaleData:
        return EphemerisEngineWarning::Code::EarthOrientationStaleData;
    case EarthOrientationSampler::Sample::WarningCode::PredictedData:
        return EphemerisEngineWarning::Code::EarthOrientationPredictedData;
    case EarthOrientationSampler::Sample::WarningCode::MissingData:
        return EphemerisEngineWarning::Code::EarthOrientationMissingData;
    case EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange:
        return EphemerisEngineWarning::Code::EarthOrientationEpochOutsideRange;
    case EarthOrientationSampler::Sample::WarningCode::InvalidInput:
        return EphemerisEngineWarning::Code::EarthOrientationInvalidInput;
    case EarthOrientationSampler::Sample::WarningCode::EstimatedData:
        return EphemerisEngineWarning::Code::EarthOrientationEstimatedData;
    }

    return EphemerisEngineWarning::Code::AccuracyDegraded;
}

void mergeTimeScaleWarningCodes(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    const auto addWarning = [&metadata, &conversion](
                                const TimeScaleConversionWarningCode sourceCode,
                                const std::optional<EphemerisEngineWarning::Code> aggregateCode = std::nullopt
                            ) noexcept {
        if (!conversion.hasWarning(sourceCode)) {
            return;
        }

        metadata.addWarning(warningCodeFor(sourceCode), conversion.diagnosticText);
        if (aggregateCode.has_value()) {
            metadata.addWarning(*aggregateCode);
        }
    };

    addWarning(
        TimeScaleConversionWarningCode::LeapSecondTableMissing, EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::LeapSecondTableStale);
    addWarning(
        TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable, EphemerisEngineWarning::Code::DataOutOfRange
    );
    addWarning(TimeScaleConversionWarningCode::LeapSecondFallbackApplied);
    addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
    addWarning(TimeScaleConversionWarningCode::InvalidInput);
    addWarning(TimeScaleConversionWarningCode::TdbApproximationApplied);
    addWarning(
        TimeScaleConversionWarningCode::EarthOrientationDataMissing,
        EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::EarthOrientationDataStale);
    addWarning(TimeScaleConversionWarningCode::EarthOrientationDataPredicted);
    addWarning(
        TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData, EphemerisEngineWarning::Code::DataOutOfRange
    );
    addWarning(TimeScaleConversionWarningCode::DeltaTFallbackApplied);
    addWarning(
        TimeScaleConversionWarningCode::DeltaTUnavailable, EphemerisEngineWarning::Code::TimeScaleDataUnavailable
    );
    addWarning(TimeScaleConversionWarningCode::EarthOrientationDataEstimated);
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

        metadata.addWarning(warningCodeFor(sourceCode), sample.diagnosticText);
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

}  // namespace skygate::ephemeris::highprecision
