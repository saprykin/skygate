#include "skygate/ephemeris/EarthOrientationProvider.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace {

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    explicit TestEphemerisDataSnapshot(std::optional<skygate::ephemeris::EphemerisTextDataAsset> asset)
        : m_asset(std::move(asset))
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationDataAsset() const override
    {
        return m_asset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_asset;
};

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidAsset()
{
    return {
        .id = "earth-orientation",
        .version = "asset-version",
        .provenance = "bundled",
        .content =
            "#@ version 2026a\n"
            "#@ source IERS Bulletin A\n"
            "#@ expires 2026-07-01\n"
            "#@ prediction_start 2026-05-01\n"
            "#@ prediction_end 2026-06-30\n"
            "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds,predicted\n"
            "2026-04-01,0.03142,0.1123,0.2187,false\n"
            "2026-05-01,0.03450,0.1180,0.2210,true\n"
            "2026-06-01,0.03725,0.1234,0.2275,true\n",
    };
}

[[nodiscard]] skygate::ephemeris::AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

}  // namespace

class EarthOrientationProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsValidDataFromSnapshot();
    void loadsIersC04Data();
    void loadsIersFinals2000AData();
    void reportsMissingData();
    void rejectsMalformedRows();
    void rejectsMissingValues();
    void rejectsPartialPredictionMetadata();
    void exposesPredictionIntervalMetadata();
    void exposesValidityRangeMetadata();
    void reportsStaleData();
    void samplesExactRows();
    void interpolatesBetweenRows();
    void samplesAtRangeBoundaries();
    void reportsOutOfRangeFallback();
    void reportsOutOfRangeFailureWhenFallbackDisallowed();
    void canTreatPredictedSamplesAsUsable();
    void reportsStaleAndPredictedSamplesAsDegraded();
    void reportsEstimatedSamplesAsDegraded();
    void reportsMissingDataFallback();
};

void EarthOrientationProviderTests::loadsValidDataFromSnapshot()
{
    const TestEphemerisDataSnapshot snapshot(makeValidAsset());
    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromSnapshot(snapshot);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Available)
    );
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QVERIFY(result.dataInfo.version == std::string{"2026a"});
    QVERIFY(result.dataInfo.provenance == std::string{"IERS Bulletin A"});
    QVERIFY(!result.dataInfo.diagnosticText.empty());

    const skygate::ephemeris::EarthOrientationTableEntry& firstEntry = result.provider->entries().front();
    QCOMPARE(firstEntry.effectiveUtcEpoch.julianDatePart1, epochForDate(2026, 4, 1).julianDatePart1);
    QCOMPARE(firstEntry.ut1MinusUtcSeconds, 0.03142);
    QCOMPARE(firstEntry.polarMotionXArcseconds, 0.1123);
    QCOMPARE(firstEntry.polarMotionYArcseconds, 0.2187);
    QVERIFY(!firstEntry.predicted);
    QVERIFY(result.provider->entries()[1].predicted);
}

void EarthOrientationProviderTests::loadsIersC04Data()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "EARTH ORIENTATION PARAMETER (EOP) PRODUCT CENTER CENTER (PARIS OBSERVATORY)\n"
                    "Date      MJD      x          y        UT1-UTC       LOD\n"
                    "(0h UTC)\n"
                    "1962  1  1  37665   0.003212   0.195335   0.0326330   0.001723  "
                    "-0.001136  -0.003667\n"
                    "1962  1  2  37666   0.004421   0.196297   0.0320540   0.001669  "
                    "-0.001163  -0.003646\n";

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{2});
    const skygate::ephemeris::EarthOrientationTableEntry& firstEntry = result.provider->entries().front();
    QCOMPARE(firstEntry.effectiveUtcDate.astronomicalYear, 1962);
    QCOMPARE(firstEntry.effectiveUtcDate.month, 1);
    QCOMPARE(firstEntry.effectiveUtcDate.day, 1);
    QCOMPARE(firstEntry.ut1MinusUtcSeconds, 0.0326330);
    QCOMPARE(firstEntry.polarMotionXArcseconds, 0.003212);
    QCOMPARE(firstEntry.polarMotionYArcseconds, 0.195335);
    QVERIFY(!firstEntry.predicted);
    QVERIFY(!firstEntry.estimated);
}

void EarthOrientationProviderTests::loadsIersFinals2000AData()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "73 1 2 41684.00 I  0.120733 0.009786  0.136966 0.015902  I 0.8084178 "
                    "0.0002710  0.0000 0.1916  P    -0.766    0.199    -0.720    0.300   "
                    ".143000   .137000   .8075000   -18.637    -3.667  \n"
                    "26 515 61175.00 P  0.170615 0.000604  0.411608 0.000484  P "
                    "0.0271380 0.0001080                 P     0.002    0.128    -0.196    "
                    "0.160                                                     \n"
                    "27 710 61596.00\n";

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{2});
    const skygate::ephemeris::EarthOrientationTableEntry& firstEntry = result.provider->entries().front();
    QCOMPARE(firstEntry.effectiveUtcDate.astronomicalYear, 1973);
    QCOMPARE(firstEntry.effectiveUtcDate.month, 1);
    QCOMPARE(firstEntry.effectiveUtcDate.day, 2);
    QCOMPARE(firstEntry.ut1MinusUtcSeconds, 0.8084178);
    QCOMPARE(firstEntry.polarMotionXArcseconds, 0.120733);
    QCOMPARE(firstEntry.polarMotionYArcseconds, 0.136966);
    QVERIFY(!firstEntry.predicted);

    const skygate::ephemeris::EarthOrientationTableEntry& lastEntry = result.provider->entries().back();
    QCOMPARE(lastEntry.effectiveUtcDate.astronomicalYear, 2026);
    QCOMPARE(lastEntry.effectiveUtcDate.month, 5);
    QCOMPARE(lastEntry.effectiveUtcDate.day, 15);
    QCOMPARE(lastEntry.ut1MinusUtcSeconds, 0.0271380);
    QCOMPARE(lastEntry.polarMotionXArcseconds, 0.170615);
    QCOMPARE(lastEntry.polarMotionYArcseconds, 0.411608);
    QVERIFY(lastEntry.predicted);
    QVERIFY(!lastEntry.estimated);
}

void EarthOrientationProviderTests::reportsMissingData()
{
    const TestEphemerisDataSnapshot snapshot(std::nullopt);
    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromSnapshot(snapshot);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Missing)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::rejectsMalformedRows()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version bad\n"
                    "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds\n"
                    "2026-04-01,0.03142,0.1123,0.2187\n"
                    "not-a-date,0.03450,0.1180,0.2210\n";

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::rejectsMissingValues()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version missing-value\n"
                    "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds\n"
                    "2026-04-01,0.03142,,0.2187\n";

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::rejectsPartialPredictionMetadata()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version partial-prediction\n"
                    "#@ prediction_end 2026-06-30\n"
                    "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds\n"
                    "2026-04-01,0.03142,0.1123,0.2187\n";

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::exposesPredictionIntervalMetadata()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.dataInfo.predictionRange.has_value());
    QVERIFY(result.dataInfo.predictionRange->id == std::string{"earth-orientation-prediction"});
    QCOMPARE(result.dataInfo.predictionRange->start.julianDatePart1, epochForDate(2026, 5, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.predictionRange->end.julianDatePart1, epochForDate(2026, 6, 30).julianDatePart1);
}

void EarthOrientationProviderTests::exposesValidityRangeMetadata()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.dataInfo.validityRange.has_value());
    QVERIFY(result.dataInfo.expiresAt.has_value());
    QVERIFY(result.dataInfo.validityRange->id == std::string{"earth-orientation"});
    QCOMPARE(result.dataInfo.validityRange->start.julianDatePart1, epochForDate(2026, 4, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.validityRange->end.julianDatePart1, epochForDate(2026, 6, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.expiresAt->julianDatePart1, epochForDate(2026, 7, 1).julianDatePart1);
}

void EarthOrientationProviderTests::reportsStaleData()
{
    skygate::ephemeris::EarthOrientationDataLoadOptions options;
    options.referenceEpoch = epochForDate(2027, 1, 1);

    const skygate::ephemeris::EarthOrientationDataLoadResult result =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset(), options);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationDataStatus::Stale)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::samplesExactRows()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 4, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Valid)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.03142);
    QCOMPARE(sample.polarMotionXArcseconds, 0.1123);
    QCOMPARE(sample.polarMotionYArcseconds, 0.2187);
    QVERIFY(!sample.predicted);
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::interpolatesBetweenRows()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 4, 16));

    QVERIFY(sample.isSuccess());
    QVERIFY(std::abs(sample.ut1MinusUtcSeconds - 0.03296) < 1.0e-10);
    QVERIFY(std::abs(sample.polarMotionXArcseconds - 0.11515) < 1.0e-10);
    QVERIFY(std::abs(sample.polarMotionYArcseconds - 0.21985) < 1.0e-10);
    QVERIFY(sample.predicted);
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Degraded)
    );
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::PredictedData));
}

void EarthOrientationProviderTests::samplesAtRangeBoundaries()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSample first =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 4, 1));
    const skygate::ephemeris::EarthOrientationSample last =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 6, 1));

    QVERIFY(first.isSuccess());
    QVERIFY(last.isSuccess());
    QVERIFY(!first.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::EpochOutsideRange));
    QVERIFY(!last.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::EpochOutsideRange));
    QCOMPARE(last.ut1MinusUtcSeconds, 0.03725);
    QVERIFY(last.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::PredictedData));
}

void EarthOrientationProviderTests::reportsOutOfRangeFallback()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 7, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Degraded)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.03725);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::EpochOutsideRange));
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::PredictedData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::reportsOutOfRangeFailureWhenFallbackDisallowed()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    skygate::ephemeris::EarthOrientationSampleOptions options;
    options.allowOutOfRangeNearestSampleFallback = false;
    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 3, 1), options);

    QVERIFY(!sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Failed)
    );
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::EpochOutsideRange));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::canTreatPredictedSamplesAsUsable()
{
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    skygate::ephemeris::EarthOrientationSampleOptions options;
    options.degradePredictedData = false;
    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 5, 1), options);

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Valid)
    );
    QVERIFY(sample.predicted);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::PredictedData));
}

void EarthOrientationProviderTests::reportsStaleAndPredictedSamplesAsDegraded()
{
    skygate::ephemeris::EarthOrientationDataLoadOptions loadOptions;
    loadOptions.referenceEpoch = epochForDate(2027, 1, 1);
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(makeValidAsset(), loadOptions);
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 5, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Degraded)
    );
    QVERIFY(sample.predicted);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::StaleData));
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::PredictedData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::reportsEstimatedSamplesAsDegraded()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content =
        "#@ version estimated\n"
        "#@ source unit test\n"
        "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds,predicted,"
        "estimated\n"
        "2026-04-01,0.03142,0.1123,0.2187,false,true\n";
    const skygate::ephemeris::EarthOrientationDataLoadResult loadResult =
        skygate::ephemeris::loadEarthOrientationDataFromTextAsset(asset);
    QVERIFY(loadResult.isSuccess());
    QVERIFY(loadResult.provider->entries().front().estimated);

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(loadResult.provider, epochForDate(2026, 4, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Degraded)
    );
    QVERIFY(sample.estimated);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::EstimatedData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::reportsMissingDataFallback()
{
    skygate::ephemeris::EarthOrientationSampleOptions options;
    options.allowMissingDataZeroFallback = true;

    const skygate::ephemeris::EarthOrientationSample sample =
        skygate::ephemeris::sampleEarthOrientation(nullptr, epochForDate(2026, 5, 1), options);

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampleStatus::Degraded)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.0);
    QCOMPARE(sample.polarMotionXArcseconds, 0.0);
    QCOMPARE(sample.polarMotionYArcseconds, 0.0);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampleWarningCode::MissingData));
    QVERIFY(!sample.diagnosticText.empty());
}

QTEST_MAIN(EarthOrientationProviderTests)

#include "EarthOrientationProviderTests.moc"
