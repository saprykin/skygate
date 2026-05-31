#include "engine/highprecision/EphemerisMetadataMerger.hpp"

#include <QtTest/QtTest>

#include <cstdint>

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;

class EphemerisMetadataMergeTests final : public QObject {
    Q_OBJECT

private slots:
    void degradedTimeScaleConversionMapsWarningCodes();
    void failedTimeScaleConversionHonorsFailurePolicy();
    void degradedEarthOrientationSampleMapsWarningCodes();
    void failedEarthOrientationSampleMapsWarningCodes();
    void markCorrectionUnavailableDegradesAndAddsWarning();
    void markCorrectionFailedFailsRecoverableStatusesAndAddsWarnings();
    void markCorrectionFailedPreservesTerminalStatusesAndAddsWarnings();
    void markCorrectionAppliedRecordsCorrection();
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
    EarthOrientationSample sample;
    sample.status = EarthOrientationSampleStatus::Degraded;
    sample.addWarning(EarthOrientationSampleWarningCode::MissingData);
    sample.addWarning(EarthOrientationSampleWarningCode::EpochOutsideRange);

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

void EphemerisMetadataMergeTests::failedEarthOrientationSampleMapsWarningCodes()
{
    EarthOrientationSample sample;
    sample.status = EarthOrientationSampleStatus::Failed;
    sample.addWarning(EarthOrientationSampleWarningCode::EpochOutsideRange);

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

QTEST_APPLESS_MAIN(EphemerisMetadataMergeTests)

#include "EphemerisMetadataMergeTests.moc"
