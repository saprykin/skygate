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
