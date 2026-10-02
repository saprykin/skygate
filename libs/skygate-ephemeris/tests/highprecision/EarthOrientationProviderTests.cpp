#include "time/CalendarTime.hpp"
#include "engine/EarthOrientationDataLoader.hpp"
#include "engine/EarthOrientationDataParser.hpp"
#include "engine/EarthOrientationSampler.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/TableBackedEarthOrientationProvider.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

[[nodiscard]] skygate::core::AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] skygate::core::CivilDateTime dateFromEpoch(const skygate::core::AstronomicalEpoch& epoch)
{
    const auto date = skygate::core::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);
    Q_ASSERT(date.has_value());
    return *date;
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
    void parsesCsvVariants_data();
    void parsesCsvVariants();
    void preservesParsingDiagnostics_data();
    void preservesParsingDiagnostics();
    void preservesAssetMetadataAndCrLf();
    void preservesDuplicateDateSampling();
    void loadsFixedColumnFinalsAndDateOnlyTail();
    void loadsWhitespaceFinals();
    void preservesExpirationBoundaries();
    void preservesPredictionAndEstimatedWarnings();
    void rejectsInvalidEpochsBeforeFallback();
    void handlesUnusableAndEmptyProviders();
    void preservesBothOutOfRangePolicies();
    void exposesParsedDataWithoutConstructingProvider();
};

void EarthOrientationProviderTests::loadsValidDataFromSnapshot()
{
    const TestEphemerisDataSnapshot snapshot(makeValidAsset());
    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromSnapshot(snapshot);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available)
    );
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QVERIFY(result.dataInfo.version == std::string{"2026a"});
    QVERIFY(result.dataInfo.provenance == std::string{"IERS Bulletin A"});
    QVERIFY(!result.dataInfo.diagnosticText.empty());

    const skygate::ephemeris::IEarthOrientationProvider::TableEntry& firstEntry = result.provider->entries().front();
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

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{2});
    const skygate::ephemeris::IEarthOrientationProvider::TableEntry& firstEntry = result.provider->entries().front();
    const skygate::core::CivilDateTime firstDate = dateFromEpoch(firstEntry.effectiveUtcEpoch);
    QCOMPARE(firstDate.astronomicalYear, 1962);
    QCOMPARE(firstDate.month, 1);
    QCOMPARE(firstDate.day, 1);
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

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{2});
    const skygate::ephemeris::IEarthOrientationProvider::TableEntry& firstEntry = result.provider->entries().front();
    const skygate::core::CivilDateTime firstDate = dateFromEpoch(firstEntry.effectiveUtcEpoch);
    QCOMPARE(firstDate.astronomicalYear, 1973);
    QCOMPARE(firstDate.month, 1);
    QCOMPARE(firstDate.day, 2);
    QCOMPARE(firstEntry.ut1MinusUtcSeconds, 0.8084178);
    QCOMPARE(firstEntry.polarMotionXArcseconds, 0.120733);
    QCOMPARE(firstEntry.polarMotionYArcseconds, 0.136966);
    QVERIFY(!firstEntry.predicted);

    const skygate::ephemeris::IEarthOrientationProvider::TableEntry& lastEntry = result.provider->entries().back();
    const skygate::core::CivilDateTime lastDate = dateFromEpoch(lastEntry.effectiveUtcEpoch);
    QCOMPARE(lastDate.astronomicalYear, 2026);
    QCOMPARE(lastDate.month, 5);
    QCOMPARE(lastDate.day, 15);
    QCOMPARE(lastEntry.ut1MinusUtcSeconds, 0.0271380);
    QCOMPARE(lastEntry.polarMotionXArcseconds, 0.170615);
    QCOMPARE(lastEntry.polarMotionYArcseconds, 0.411608);
    QVERIFY(lastEntry.predicted);
    QVERIFY(!lastEntry.estimated);
}

void EarthOrientationProviderTests::reportsMissingData()
{
    const TestEphemerisDataSnapshot snapshot(std::nullopt);
    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromSnapshot(snapshot);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Missing)
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

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::rejectsMissingValues()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version missing-value\n"
                    "effective_utc_date,ut1_minus_utc_seconds,polar_motion_x_arcseconds,polar_motion_y_arcseconds\n"
                    "2026-04-01,0.03142,,0.2187\n";

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed)
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

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::exposesPredictionIntervalMetadata()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.dataInfo.predictionRange.has_value());
    QVERIFY(result.dataInfo.predictionRange->id == std::string{"earth-orientation-prediction"});
    QCOMPARE(result.dataInfo.predictionRange->start.julianDatePart1, epochForDate(2026, 5, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.predictionRange->end.julianDatePart1, epochForDate(2026, 6, 30).julianDatePart1);
}

void EarthOrientationProviderTests::exposesValidityRangeMetadata()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());

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
    skygate::ephemeris::EarthOrientationDataLoader::Options options;
    options.referenceEpoch = epochForDate(2027, 1, 1);

    const skygate::ephemeris::EarthOrientationDataLoader::Result result =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset(), options);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IEarthOrientationProvider::DataStatus::Stale)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void EarthOrientationProviderTests::samplesExactRows()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 4, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Valid)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.03142);
    QCOMPARE(sample.polarMotionXArcseconds, 0.1123);
    QCOMPARE(sample.polarMotionYArcseconds, 0.2187);
    QVERIFY(!sample.predicted);
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::interpolatesBetweenRows()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 4, 16));

    QVERIFY(sample.isSuccess());
    QVERIFY(std::abs(sample.ut1MinusUtcSeconds - 0.03296) < 1.0e-10);
    QVERIFY(std::abs(sample.polarMotionXArcseconds - 0.11515) < 1.0e-10);
    QVERIFY(std::abs(sample.polarMotionYArcseconds - 0.21985) < 1.0e-10);
    QVERIFY(sample.predicted);
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded)
    );
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::PredictedData));
}

void EarthOrientationProviderTests::samplesAtRangeBoundaries()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSampler::Sample first =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 4, 1));
    const skygate::ephemeris::EarthOrientationSampler::Sample last =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 6, 1));

    QVERIFY(first.isSuccess());
    QVERIFY(last.isSuccess());
    QVERIFY(!first.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QVERIFY(!last.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QCOMPARE(last.ut1MinusUtcSeconds, 0.03725);
    QVERIFY(last.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::PredictedData));
}

void EarthOrientationProviderTests::reportsOutOfRangeFallback()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 7, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.03725);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::PredictedData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::reportsOutOfRangeFailureWhenFallbackDisallowed()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    skygate::ephemeris::EarthOrientationSampler::Options options;
    options.allowOutOfRangeNearestSampleFallback = false;
    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 3, 1), options);

    QVERIFY(!sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Failed)
    );
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::canTreatPredictedSamplesAsUsable()
{
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset());
    QVERIFY(loadResult.isSuccess());

    skygate::ephemeris::EarthOrientationSampler::Options options;
    options.degradePredictedData = false;
    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 5, 1), options);

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Valid)
    );
    QVERIFY(sample.predicted);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::PredictedData));
}

void EarthOrientationProviderTests::reportsStaleAndPredictedSamplesAsDegraded()
{
    skygate::ephemeris::EarthOrientationDataLoader::Options loadOptions;
    loadOptions.referenceEpoch = epochForDate(2027, 1, 1);
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(makeValidAsset(), loadOptions);
    QVERIFY(loadResult.isSuccess());

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 5, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded)
    );
    QVERIFY(sample.predicted);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::StaleData));
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::PredictedData));
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
    const skygate::ephemeris::EarthOrientationDataLoader::Result loadResult =
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(loadResult.isSuccess());
    QVERIFY(loadResult.provider->entries().front().estimated);

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(loadResult.provider, epochForDate(2026, 4, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded)
    );
    QVERIFY(sample.estimated);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::EstimatedData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::reportsMissingDataFallback()
{
    skygate::ephemeris::EarthOrientationSampler::Options options;
    options.allowMissingDataZeroFallback = true;

    const skygate::ephemeris::EarthOrientationSampler::Sample sample =
        skygate::ephemeris::EarthOrientationSampler::sample(nullptr, epochForDate(2026, 5, 1), options);

    QVERIFY(sample.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(sample.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded)
    );
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.0);
    QCOMPARE(sample.polarMotionXArcseconds, 0.0);
    QCOMPARE(sample.polarMotionYArcseconds, 0.0);
    QVERIFY(sample.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::MissingData));
    QVERIFY(!sample.diagnosticText.empty());
}

void EarthOrientationProviderTests::parsesCsvVariants_data()
{
    QTest::addColumn<QString>("flags");
    QTest::addColumn<bool>("predicted");
    QTest::addColumn<bool>("estimated");
    QTest::newRow("four-columns") << "" << false << false;
    QTest::newRow("prediction-true") << ",true" << true << false;
    QTest::newRow("prediction-false") << ",false" << false << false;
    QTest::newRow("numeric-flags") << ",1,0" << true << false;
    QTest::newRow("estimated") << ",false,true" << false << true;
    QTest::newRow("both") << ", true , 1 " << true << true;
}

void EarthOrientationProviderTests::parsesCsvVariants()
{
    QFETCH(QString, flags);
    QFETCH(bool, predicted);
    QFETCH(bool, estimated);
    auto asset = makeValidAsset();
    asset.content = "2026-04-01,0.1,0.2,0.3" + flags.toStdString();
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.provider->entries().size(), std::size_t{1});
    const auto& entry = result.provider->entries().front();
    QCOMPARE(entry.predicted, predicted);
    QCOMPARE(entry.estimated, estimated);
    QCOMPARE(entry.ut1MinusUtcSeconds, 0.1);
    QCOMPARE(entry.polarMotionXArcseconds, 0.2);
    QCOMPARE(entry.polarMotionYArcseconds, 0.3);
}

void EarthOrientationProviderTests::preservesParsingDiagnostics_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<QString>("diagnostic");
    QTest::newRow("empty") << "" << "Earth-orientation data contains no rows.";
    QTest::newRow("comments") << "# comment\n\n" << "Earth-orientation data contains no rows.";
    QTest::newRow("unsorted") << "2026-04-02,0.1,0.2,0.3\n2026-04-01,0.1,0.2,0.3\n"
                              << "Earth-orientation rows are not sorted by UTC date.";
    QTest::newRow("line-number") << "# comment\n\n2026-04-01,invalid,0.2,0.3\n"
                                 << "Earth-orientation data contains a malformed row at line 3.";
    QTest::newRow("infinity") << "2026-04-01,inf,0.2,0.3"
                              << "Earth-orientation data contains a malformed row at line 1.";
    QTest::newRow("nan") << "2026-04-01,0.1,nan,0.3"
                         << "Earth-orientation data contains a malformed row at line 1.";
    QTest::newRow("invalid-flag") << "2026-04-01,0.1,0.2,0.3,yes"
                                  << "Earth-orientation data contains a malformed row at line 1.";
    QTest::newRow("invalid-estimated-flag") << "2026-04-01,0.1,0.2,0.3,false,yes"
                                            << "Earth-orientation data contains a malformed row at line 1.";
    QTest::newRow("expires") << "#@ expires invalid\n2026-04-01,0.1,0.2,0.3"
                             << "Earth-orientation data contains malformed expiration metadata at line 1.";
    QTest::newRow(
        "prediction-start"
    ) << "#@ prediction_start invalid\n2026-04-01,0.1,0.2,0.3"
      << "Earth-orientation data contains malformed prediction start metadata at line 1.";
    QTest::newRow("prediction-end") << "#@ prediction_end invalid\n2026-04-01,0.1,0.2,0.3"
                                    << "Earth-orientation data contains malformed prediction end metadata at line 1.";
    QTest::newRow("partial-range") << "#@ prediction_start 2026-04-01\n2026-04-01,0.1,0.2,0.3"
                                   << "Earth-orientation prediction metadata must include a valid start and end range.";
    QTest::newRow(
        "reversed-range"
    ) << "#@ prediction_start 2026-04-02\n#@ prediction_end 2026-04-01\n2026-04-01,0.1,0.2,0.3"
      << "Earth-orientation prediction metadata must include a valid start and end range.";
}

void EarthOrientationProviderTests::preservesParsingDiagnostics()
{
    QFETCH(QString, content);
    QFETCH(QString, diagnostic);
    auto asset = makeValidAsset();
    asset.content = content.toStdString();
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(result.dataInfo.status, skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed);
    QCOMPARE(QString::fromStdString(result.dataInfo.diagnosticText), diagnostic);
}

void EarthOrientationProviderTests::preservesAssetMetadataAndCrLf()
{
    auto asset = makeValidAsset();
    asset.content = "  # comment\r\n\r\n#@ version\r\n#@ source\r\n#@ unknown ignored\r\n"
                    "effective_utc_date,ut1_minus_utc_seconds,x,y\r\n 2026-04-01 , 0.1 , 0.2 , 0.3 \r\n";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.dataInfo.version, asset.version);
    QCOMPARE(result.dataInfo.provenance, asset.provenance);
    QCOMPARE(result.dataInfo.diagnosticText, std::string{"Earth-orientation data loaded."});
    QCOMPARE(result.provider->dataInfo().diagnosticText, result.dataInfo.diagnosticText);
}

void EarthOrientationProviderTests::preservesDuplicateDateSampling()
{
    auto asset = makeValidAsset();
    asset.content = "2026-04-01,0.1,0.2,0.3\n2026-04-01,0.5,0.6,0.7\n2026-04-03,0.9,1.0,1.1";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    const auto exact = skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, 1));
    QCOMPARE(exact.ut1MinusUtcSeconds, 0.1);
    const auto between = skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, 2));
    QCOMPARE(between.ut1MinusUtcSeconds, 0.7);
    QCOMPARE(between.polarMotionXArcseconds, 0.8);
    QVERIFY(std::abs(between.polarMotionYArcseconds - 0.9) < 1.0e-15);
}

void EarthOrientationProviderTests::loadsFixedColumnFinalsAndDateOnlyTail()
{
    auto asset = makeValidAsset();
    std::string line(70U, ' ');
    line.replace(0U, 6U, "26 415");
    line.replace(7U, 8U, "61145.00");
    line[16U] = 'P';
    line.replace(18U, 9U, " 0.120733");
    line.replace(37U, 9U, " 0.136966");
    line[57U] = 'I';
    line.replace(58U, 10U, " 0.8084178");
    std::string tail(70U, ' ');
    tail.replace(0U, 6U, "26 416");
    tail.replace(7U, 8U, "61146.00");
    asset.content = line + "\r\n" + tail + "\r\n";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.provider->entries().size(), std::size_t{1});
    const auto& entry = result.provider->entries().front();
    const skygate::core::CivilDateTime entryDate = dateFromEpoch(entry.effectiveUtcEpoch);
    QCOMPARE(entryDate.astronomicalYear, 2026);
    QCOMPARE(entryDate.month, 4);
    QCOMPARE(entryDate.day, 15);
    QCOMPARE(entry.ut1MinusUtcSeconds, 0.8084178);
    QCOMPARE(entry.polarMotionXArcseconds, 0.120733);
    QCOMPARE(entry.polarMotionYArcseconds, 0.136966);
    QVERIFY(entry.predicted);
}

void EarthOrientationProviderTests::loadsWhitespaceFinals()
{
    auto asset = makeValidAsset();
    asset.content = "26 4 15 61145.00 I 0.1 0.01 0.2 0.01 P 0.3\n26 4 16 61146.00\n";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.provider->entries().size(), std::size_t{1});
    QCOMPARE(result.provider->entries().front().ut1MinusUtcSeconds, 0.3);
    QVERIFY(result.provider->entries().front().predicted);
}

void EarthOrientationProviderTests::preservesExpirationBoundaries()
{
    auto asset = makeValidAsset();
    asset.content = "#@ expires 2026-04-02\n2026-04-01,0.1,0.2,0.3\n2026-04-03,0.3,0.4,0.5";
    skygate::ephemeris::EarthOrientationDataLoader::Options options;
    options.referenceEpoch = epochForDate(2026, 4, 2);
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset, options);
    QVERIFY(result.isSuccess());
    QCOMPARE(result.dataInfo.status, skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available);
    const auto atExpiry = skygate::ephemeris::EarthOrientationSampler::sample(result.provider, *options.referenceEpoch);
    QVERIFY(!atExpiry.hasWarning(skygate::ephemeris::EarthOrientationSampler::Sample::WarningCode::StaleData));
    const auto afterExpiry =
        skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, 3));
    QCOMPARE(afterExpiry.warningCodeMask, std::uint32_t{1});
    QCOMPARE(afterExpiry.diagnosticText, std::string{"Earth-orientation sample resolved with degraded metadata."});
    options.referenceEpoch = epochForDate(2026, 4, 3);
    const auto stale = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset, options);
    QCOMPARE(stale.dataInfo.status, skygate::ephemeris::IEarthOrientationProvider::DataStatus::Stale);
    QCOMPARE(stale.dataInfo.diagnosticText, std::string{"Earth-orientation data is stale for the reference epoch."});
    QCOMPARE(stale.provider->dataInfo().status, stale.dataInfo.status);
    options.referenceEpoch->timeScale = skygate::core::TimeScale::Tt;
    QCOMPARE(
        skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset, options).dataInfo.status,
        skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available
    );
}

void EarthOrientationProviderTests::preservesPredictionAndEstimatedWarnings()
{
    auto asset = makeValidAsset();
    asset.content = "#@ prediction_start 2026-04-02\n#@ prediction_end 2026-04-03\n"
                    "2026-04-01,0.1,0.2,0.3,false,false\n2026-04-04,0.4,0.5,0.6,false,false";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    for (const int day : {2, 3}) {
        const auto sample =
            skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, day));
        QCOMPARE(sample.warningCodeMask, std::uint32_t{2});
        QCOMPARE(sample.status, skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded);
        QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample interpolated with degraded metadata."});
    }
    QVERIFY(!skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, 4)).predicted);
    auto info = result.dataInfo;
    info.status = skygate::ephemeris::IEarthOrientationProvider::DataStatus::Estimated;
    info.expiresAt = epochForDate(2026, 4, 1);
    const skygate::ephemeris::TableBackedEarthOrientationProvider provider(
        info,
        std::vector<skygate::ephemeris::IEarthOrientationProvider::TableEntry>(
            result.provider->entries().begin(), result.provider->entries().end()
        )
    );
    const auto combined = skygate::ephemeris::EarthOrientationSampler::sample(&provider, epochForDate(2026, 4, 2));
    QCOMPARE(combined.warningCodeMask, std::uint32_t{35});
    QVERIFY(combined.estimated);
    QVERIFY(combined.predicted);
}

void EarthOrientationProviderTests::rejectsInvalidEpochsBeforeFallback()
{
    skygate::ephemeris::EarthOrientationSampler::Options options;
    options.allowMissingDataZeroFallback = true;
    auto epoch = epochForDate(2026, 4, 1);
    epoch.timeScale = skygate::core::TimeScale::Tt;
    const auto nonUtc = skygate::ephemeris::EarthOrientationSampler::sample(nullptr, epoch, options);
    QCOMPARE(nonUtc.status, skygate::ephemeris::EarthOrientationSampler::Sample::Status::Failed);
    QCOMPARE(nonUtc.warningCodeMask, std::uint32_t{16});
    QCOMPARE(nonUtc.diagnosticText, std::string{"Earth-orientation sampling requires a finite UTC epoch."});
    epoch.timeScale = skygate::core::TimeScale::Utc;
    epoch.julianDatePart2 = std::numeric_limits<double>::quiet_NaN();
    const auto nonFinite = skygate::ephemeris::EarthOrientationSampler::sample(nullptr, epoch, options);
    QCOMPARE(nonFinite.warningCodeMask, nonUtc.warningCodeMask);
    QCOMPARE(nonFinite.diagnosticText, nonUtc.diagnosticText);
}

void EarthOrientationProviderTests::handlesUnusableAndEmptyProviders()
{
    for (const auto status :
         {skygate::ephemeris::IEarthOrientationProvider::DataStatus::Missing,
          skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed,
          skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available}) {
        skygate::ephemeris::IEarthOrientationProvider::DataInfo info;
        info.status = status;
        const auto provider = std::make_shared<skygate::ephemeris::TableBackedEarthOrientationProvider>(
            info, std::vector<skygate::ephemeris::IEarthOrientationProvider::TableEntry>{}
        );
        const auto failed = skygate::ephemeris::EarthOrientationSampler::sample(provider, epochForDate(2026, 4, 1));
        QVERIFY(!failed.isSuccess());
        QCOMPARE(failed.warningCodeMask, std::uint32_t{4});
        QCOMPARE(failed.diagnosticText, std::string{"Earth-orientation data is unavailable for the requested epoch."});
        skygate::ephemeris::EarthOrientationSampler::Options options;
        options.allowMissingDataZeroFallback = true;
        const auto fallback =
            skygate::ephemeris::EarthOrientationSampler::sample(provider, epochForDate(2026, 4, 1), options);
        QVERIFY(fallback.isSuccess());
        QCOMPARE(fallback.warningCodeMask, failed.warningCodeMask);
        QCOMPARE(fallback.diagnosticText, std::string{"Earth-orientation data used a degraded zero-value fallback."});
    }
}

void EarthOrientationProviderTests::preservesBothOutOfRangePolicies()
{
    auto asset = makeValidAsset();
    asset.content = "2026-04-02,0.1,0.2,0.3\n2026-04-03,0.4,0.5,0.6";
    const auto result = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(result.isSuccess());
    for (const int day : {1, 4}) {
        const auto fallback =
            skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, day));
        QCOMPARE(fallback.status, skygate::ephemeris::EarthOrientationSampler::Sample::Status::Degraded);
        QCOMPARE(fallback.warningCodeMask, std::uint32_t{8});
        QCOMPARE(fallback.ut1MinusUtcSeconds, day == 1 ? 0.1 : 0.4);
        QCOMPARE(
            fallback.diagnosticText,
            day == 1 ? std::string{"Earth-orientation sample used the first available row as a degraded fallback."}
                     : std::string{"Earth-orientation sample used the last available row as a degraded fallback."}
        );
        skygate::ephemeris::EarthOrientationSampler::Options options;
        options.allowOutOfRangeNearestSampleFallback = false;
        const auto failed =
            skygate::ephemeris::EarthOrientationSampler::sample(result.provider, epochForDate(2026, 4, day), options);
        QVERIFY(!failed.isSuccess());
        QCOMPARE(failed.warningCodeMask, fallback.warningCodeMask);
        QCOMPARE(
            failed.diagnosticText,
            day == 1 ? std::string{"Requested epoch is before the first Earth-orientation row."}
                     : std::string{"Requested epoch is after the last Earth-orientation row."}
        );
    }
}

void EarthOrientationProviderTests::exposesParsedDataWithoutConstructingProvider()
{
    auto asset = makeValidAsset();
    const auto parsed = skygate::ephemeris::EarthOrientationDataParser::parse(asset);
    QVERIFY(parsed.isSuccess());
    QCOMPARE(parsed.entries.size(), std::size_t{3});
    QCOMPARE(parsed.dataInfo.status, skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available);
    const auto loaded = skygate::ephemeris::EarthOrientationDataLoader::loadFromTextAsset(asset);
    QVERIFY(loaded.isSuccess());
    QCOMPARE(parsed.dataInfo.version, loaded.dataInfo.version);
    QCOMPARE(parsed.dataInfo.provenance, loaded.dataInfo.provenance);
    QCOMPARE(parsed.dataInfo.diagnosticText, loaded.dataInfo.diagnosticText);
    QCOMPARE(parsed.entries.front().ut1MinusUtcSeconds, loaded.provider->entries().front().ut1MinusUtcSeconds);
    QCOMPARE(parsed.dataInfo.validityRange->start.sortKey(), loaded.dataInfo.validityRange->start.sortKey());
    asset.content = "2026-04-01,0.1,0.2,0.3\ninvalid,row";
    const auto malformed = skygate::ephemeris::EarthOrientationDataParser::parse(asset);
    QVERIFY(!malformed.isSuccess());
    QVERIFY(malformed.entries.empty());
    QCOMPARE(malformed.dataInfo.status, skygate::ephemeris::IEarthOrientationProvider::DataStatus::Malformed);
    QCOMPARE(
        malformed.dataInfo.diagnosticText, std::string{"Earth-orientation data contains a malformed row at line 2."}
    );
}

QTEST_MAIN(EarthOrientationProviderTests)

#include "EarthOrientationProviderTests.moc"
