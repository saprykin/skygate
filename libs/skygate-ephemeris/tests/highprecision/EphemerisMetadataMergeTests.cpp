#include "engine/highprecision/EphemerisMetadataMerger.hpp"
#include "TestCalcephKernel.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <string>
#include <vector>

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

class EphemerisMetadataMergeTests final : public QObject {
    Q_OBJECT

private slots:
    void degradedTimeScaleConversionMapsWarningCodes();
    void degradedTimeScaleConversionPreservesSpecificCodesAndText();
    void failedTimeScaleConversionHonorsFailurePolicy();
    void degradedEarthOrientationSampleMapsWarningCodes();
    void degradedEarthOrientationSamplePreservesSpecificCodesAndText();
    void failedEarthOrientationSampleMapsWarningCodes();
    void mergeKernelDiagnosticsAppendsUnreportedDiagnostics();
    void markCorrectionUnavailableDegradesAndAddsWarning();
    void markCorrectionFailedFailsRecoverableStatusesAndAddsWarnings();
    void markCorrectionFailedPreservesTerminalStatusesAndAddsWarnings();
    void markCorrectionAppliedRecordsCorrection();
    void mergePropagatesFullResultStatuses();
    void mergeRespectsDegradedAndFailedOnlyStatusPolicy();
    void mergeCombinesCorrectionsWarningsAndOptionalMetadata();
    void mergeHonorsMetadataOptionToggles();
};

void EphemerisMetadataMergeTests::degradedTimeScaleConversionMapsWarningCodes()
{
    TimeScaleConversionResult conversion;
    conversion.status = TimeScaleConversionStatus::Degraded;
    conversion.addWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing);
    conversion.addWarning(TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData);

    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);

    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
}

void EphemerisMetadataMergeTests::degradedTimeScaleConversionPreservesSpecificCodesAndText()
{
    TimeScaleConversionResult conversion;
    conversion.status = TimeScaleConversionStatus::Degraded;
    conversion.diagnosticText = "degraded leap-second lookup";
    conversion.addWarning(TimeScaleConversionWarningCode::LeapSecondTableStale);
    conversion.addWarning(TimeScaleConversionWarningCode::DeltaTFallbackApplied);

    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);

    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::LeapSecondTableStale));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::DeltaTFallbackApplied));
    QVERIFY(!metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(!metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));

    bool foundLeapSecondText = false;
    for (const EphemerisEngineWarning::Detail& detail : metadata.warningDetails) {
        if (detail.code == EphemerisEngineWarning::Code::LeapSecondTableStale
            && detail.text == conversion.diagnosticText) {
            foundLeapSecondText = true;
        }
    }
    QVERIFY(foundLeapSecondText);
}

void EphemerisMetadataMergeTests::failedTimeScaleConversionHonorsFailurePolicy()
{
    TimeScaleConversionResult conversion;
    conversion.status = TimeScaleConversionStatus::Failed;
    conversion.addWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable);

    EphemerisEngineQueryResult degradedMetadata;
    EphemerisMetadataMerger::mergeTimeScale(degradedMetadata, conversion, EphemerisMetadataFailurePolicy::MarkDegraded);

    QCOMPARE(
        static_cast<std::uint8_t>(degradedMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(degradedMetadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(degradedMetadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));

    EphemerisEngineQueryResult failedMetadata;
    EphemerisMetadataMerger::mergeTimeScale(failedMetadata, conversion, EphemerisMetadataFailurePolicy::MarkFailed);

    QCOMPARE(
        static_cast<std::uint8_t>(failedMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(failedMetadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(failedMetadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
}

void EphemerisMetadataMergeTests::degradedEarthOrientationSampleMapsWarningCodes()
{
    EarthOrientationSampler::Sample sample;
    sample.status = EarthOrientationSampler::Sample::Status::Degraded;
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::MissingData);
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);

    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::mergeEarthOrientation(metadata, sample);

    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
}

void EphemerisMetadataMergeTests::degradedEarthOrientationSamplePreservesSpecificCodesAndText()
{
    EarthOrientationSampler::Sample sample;
    sample.status = EarthOrientationSampler::Sample::Status::Degraded;
    sample.diagnosticText = "Earth-orientation data is stale.";
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::StaleData);
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData);

    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::mergeEarthOrientation(metadata, sample);

    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::EarthOrientationStaleData));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::EarthOrientationPredictedData));
    QVERIFY(!metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(!metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));

    bool foundStaleText = false;
    for (const EphemerisEngineWarning::Detail& detail : metadata.warningDetails) {
        if (detail.code == EphemerisEngineWarning::Code::EarthOrientationStaleData
            && detail.text == sample.diagnosticText) {
            foundStaleText = true;
        }
    }
    QVERIFY(foundStaleText);
}

void EphemerisMetadataMergeTests::failedEarthOrientationSampleMapsWarningCodes()
{
    EarthOrientationSampler::Sample sample;
    sample.status = EarthOrientationSampler::Sample::Status::Failed;
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);

    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::mergeEarthOrientation(metadata, sample);

    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(!metadata.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable));
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
}

void EphemerisMetadataMergeTests::markCorrectionUnavailableDegradesAndAddsWarning()
{
    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::markCorrectionUnavailable(metadata, EphemerisCorrectionFlags::earthOrientation());

    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(
        EphemerisCorrectionFlags::has(metadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation())
    );
}

void EphemerisMetadataMergeTests::markCorrectionFailedFailsRecoverableStatusesAndAddsWarnings()
{
    EphemerisEngineQueryResult validMetadata;
    EphemerisMetadataMerger::markCorrectionFailed(validMetadata, EphemerisCorrectionFlags::precessionNutation());

    QCOMPARE(
        static_cast<std::uint8_t>(validMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(validMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(validMetadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            validMetadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );

    EphemerisEngineQueryResult degradedMetadata;
    degradedMetadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    EphemerisMetadataMerger::markCorrectionFailed(degradedMetadata, EphemerisCorrectionFlags::earthOrientation());

    QCOMPARE(
        static_cast<std::uint8_t>(degradedMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(degradedMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(degradedMetadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            degradedMetadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );
}

void EphemerisMetadataMergeTests::markCorrectionFailedPreservesTerminalStatusesAndAddsWarnings()
{
    EphemerisEngineQueryResult outOfRangeMetadata;
    outOfRangeMetadata.status = EphemerisEngineQueryStatus::Type::OutOfRange;
    EphemerisMetadataMerger::markCorrectionFailed(outOfRangeMetadata, EphemerisCorrectionFlags::precessionNutation());

    QCOMPARE(
        static_cast<std::uint8_t>(outOfRangeMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::OutOfRange)
    );
    QVERIFY(outOfRangeMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(outOfRangeMetadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            outOfRangeMetadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );

    EphemerisEngineQueryResult unsupportedMetadata;
    unsupportedMetadata.status = EphemerisEngineQueryStatus::Type::Unsupported;
    EphemerisMetadataMerger::markCorrectionFailed(unsupportedMetadata, EphemerisCorrectionFlags::earthOrientation());

    QCOMPARE(
        static_cast<std::uint8_t>(unsupportedMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
    );
    QVERIFY(unsupportedMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(unsupportedMetadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            unsupportedMetadata.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()
        )
    );

    EphemerisEngineQueryResult failedMetadata;
    failedMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
    EphemerisMetadataMerger::markCorrectionFailed(failedMetadata, EphemerisCorrectionFlags::precessionNutation());

    QCOMPARE(
        static_cast<std::uint8_t>(failedMetadata.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(failedMetadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
    QVERIFY(failedMetadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(
        EphemerisCorrectionFlags::has(
            failedMetadata.unavailableCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
}

void EphemerisMetadataMergeTests::markCorrectionAppliedRecordsCorrection()
{
    EphemerisEngineQueryResult metadata;
    EphemerisMetadataMerger::markCorrectionApplied(metadata, EphemerisCorrectionFlags::precessionNutation());

    QCOMPARE(
        static_cast<std::uint32_t>(metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::precessionNutation())
    );
    QCOMPARE(
        static_cast<std::uint8_t>(metadata.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );
}

void EphemerisMetadataMergeTests::mergePropagatesFullResultStatuses()
{
    EphemerisEngineQueryResult target;
    EphemerisEngineQueryResult failed;
    failed.status = EphemerisEngineQueryStatus::Type::Failed;
    EphemerisMetadataMerger::merge(target, failed);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );

    target = {};
    EphemerisEngineQueryResult outOfRange;
    outOfRange.status = EphemerisEngineQueryStatus::Type::OutOfRange;
    EphemerisMetadataMerger::merge(target, outOfRange);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::OutOfRange)
    );

    target = {};
    EphemerisEngineQueryResult unsupported;
    unsupported.status = EphemerisEngineQueryStatus::Type::Unsupported;
    EphemerisMetadataMerger::merge(target, unsupported);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status),
        static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Unsupported)
    );

    target = {};
    EphemerisEngineQueryResult degraded;
    degraded.status = EphemerisEngineQueryStatus::Type::Degraded;
    EphemerisMetadataMerger::merge(target, degraded);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );

    target = {};
    target.status = EphemerisEngineQueryStatus::Type::Degraded;
    EphemerisEngineQueryResult valid;
    EphemerisMetadataMerger::merge(target, valid);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
}

void EphemerisMetadataMergeTests::mergeRespectsDegradedAndFailedOnlyStatusPolicy()
{
    EphemerisMetadataMergeOptions options;
    options.statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly;

    EphemerisEngineQueryResult target;
    EphemerisEngineQueryResult outOfRange;
    outOfRange.status = EphemerisEngineQueryStatus::Type::OutOfRange;
    EphemerisMetadataMerger::merge(target, outOfRange, options);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );

    target = {};
    EphemerisEngineQueryResult unsupported;
    unsupported.status = EphemerisEngineQueryStatus::Type::Unsupported;
    EphemerisMetadataMerger::merge(target, unsupported, options);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Valid)
    );

    target = {};
    EphemerisEngineQueryResult failed;
    failed.status = EphemerisEngineQueryStatus::Type::Failed;
    EphemerisMetadataMerger::merge(target, failed, options);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Failed)
    );

    target = {};
    EphemerisEngineQueryResult degraded;
    degraded.status = EphemerisEngineQueryStatus::Type::Degraded;
    EphemerisMetadataMerger::merge(target, degraded, options);
    QCOMPARE(
        static_cast<std::uint8_t>(target.status), static_cast<std::uint8_t>(EphemerisEngineQueryStatus::Type::Degraded)
    );
}

void EphemerisMetadataMergeTests::mergeCombinesCorrectionsWarningsAndOptionalMetadata()
{
    EphemerisEngineQueryResult source;
    source.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    source.addWarning(EphemerisEngineWarning::Code::AccuracyDegraded);
    source.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
    source.unavailableCorrections = EphemerisCorrectionFlags::earthOrientation();
    source.dataSourceProvenance = "source-provenance";
    source.effectiveDataValidityRange = EphemerisDateRange{
        .id = "source-range",
        .displayName = "Source range",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
    source.estimatedAngularUncertaintyArcsec = 0.5;

    EphemerisEngineQueryResult target;
    EphemerisMetadataMerger::merge(target, source);

    QVERIFY(target.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(target.hasWarning(EphemerisEngineWarning::Code::AccuracyDegraded));
    QVERIFY(EphemerisCorrectionFlags::has(target.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()));
    QVERIFY(EphemerisCorrectionFlags::has(target.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation()));
    QCOMPARE(target.dataSourceProvenance, std::string{"source-provenance"});
    QVERIFY(target.effectiveDataValidityRange.has_value());
    QCOMPARE(target.effectiveDataValidityRange->id, std::string{"source-range"});
    QVERIFY(target.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*target.estimatedAngularUncertaintyArcsec, 0.5);
}

void EphemerisMetadataMergeTests::mergeHonorsMetadataOptionToggles()
{
    EphemerisMetadataMergeOptions options;
    options.mergeCorrections = false;
    options.mergeProvenance = false;
    options.mergeValidityRange = false;
    options.mergeAngularUncertainty = false;

    EphemerisEngineQueryResult source;
    source.addWarning(EphemerisEngineWarning::Code::DataOutOfRange);
    source.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
    source.unavailableCorrections = EphemerisCorrectionFlags::earthOrientation();
    source.dataSourceProvenance = "source-provenance";
    source.effectiveDataValidityRange = EphemerisDateRange{
        .id = "source-range",
        .displayName = "Source range",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
    source.estimatedAngularUncertaintyArcsec = 0.5;

    EphemerisEngineQueryResult target;
    target.dataSourceProvenance = "target-provenance";
    target.effectiveDataValidityRange = EphemerisDateRange{
        .id = "target-range",
        .displayName = "Target range",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = TimeScale::Tdb},
    };
    target.estimatedAngularUncertaintyArcsec = 1.0;

    EphemerisMetadataMerger::merge(target, source, options);

    QVERIFY(target.hasWarning(EphemerisEngineWarning::Code::DataOutOfRange));
    QVERIFY(!EphemerisCorrectionFlags::has(target.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()));
    QVERIFY(
        !EphemerisCorrectionFlags::has(target.unavailableCorrections, EphemerisCorrectionFlags::earthOrientation())
    );
    QCOMPARE(target.dataSourceProvenance, std::string{"target-provenance"});
    QCOMPARE(target.effectiveDataValidityRange->id, std::string{"target-range"});
    QCOMPARE(*target.estimatedAngularUncertaintyArcsec, 1.0);
}

void EphemerisMetadataMergeTests::mergeKernelDiagnosticsAppendsUnreportedDiagnostics()
{
    skygate::ephemeris::tests::TestCalcephKernel kernel;
    kernel.setDiagnostics({"calceph compute failed", "already reported"});

    EphemerisEngineQueryResult metadata;
    metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed, "already reported");

    EphemerisMetadataMerger::mergeKernelDiagnostics(metadata, kernel);

    QVERIFY(metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
    QVERIFY(metadata.warningDetails.size() == 2U);

    bool foundCalcephFailure = false;
    for (const EphemerisEngineWarning::Detail& detail : metadata.warningDetails) {
        if (detail.text == "calceph compute failed") {
            foundCalcephFailure = true;
        }
    }
    QVERIFY(foundCalcephFailure);
}

QTEST_APPLESS_MAIN(EphemerisMetadataMergeTests)

#include "EphemerisMetadataMergeTests.moc"
