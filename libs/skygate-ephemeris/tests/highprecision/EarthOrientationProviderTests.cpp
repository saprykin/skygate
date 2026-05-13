#include "skygate/ephemeris/EarthOrientationProvider.hpp"

#include <QtTest/QtTest>

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
    const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = year,
        .month = month,
        .day = day,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

}  // namespace

class EarthOrientationProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsValidDataFromSnapshot();
    void reportsMissingData();
    void rejectsMalformedRows();
    void rejectsMissingValues();
    void rejectsPartialPredictionMetadata();
    void exposesPredictionIntervalMetadata();
    void exposesValidityRangeMetadata();
    void reportsStaleData();
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

QTEST_MAIN(EarthOrientationProviderTests)

#include "EarthOrientationProviderTests.moc"
