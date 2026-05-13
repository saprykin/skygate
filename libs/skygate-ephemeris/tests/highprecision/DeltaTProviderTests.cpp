#include "skygate/ephemeris/DeltaTProvider.hpp"

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

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTDataAsset() const override
    {
        return m_asset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_asset;
};

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidAsset()
{
    return {
        .id = "delta-t",
        .version = "asset-version",
        .provenance = "bundled",
        .content = "#@ version 2026a\n"
                   "#@ source IERS Rapid Service plus historical model\n"
                   "#@ expires 2027-01-01\n"
                   "#@ ancient_fallback_start -13200-01-01\n"
                   "#@ ancient_fallback_end 1600-01-01\n"
                   "#@ ancient_fallback_source Morrison-Stephenson model\n"
                   "#@ ancient_fallback_delta_t_seconds 12000.5\n"
                   "#@ ancient_fallback_uncertainty_seconds 7200\n"
                   "effective_utc_date,delta_t_seconds\n"
                   "1900-01-01,-2.72\n"
                   "2000-01-01,63.83\n"
                   "2026-01-01,69.20\n",
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

class DeltaTProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsPresentDataFromSnapshot();
    void reportsMissingData();
    void rejectsMalformedRows();
    void exposesAncientFallbackMetadata();
    void exposesValidityRangeMetadata();
    void reportsStaleData();
};

void DeltaTProviderTests::loadsPresentDataFromSnapshot()
{
    const TestEphemerisDataSnapshot snapshot(makeValidAsset());
    const skygate::ephemeris::DeltaTDataLoadResult result = skygate::ephemeris::loadDeltaTDataFromSnapshot(snapshot);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTDataStatus::Available)
    );
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QVERIFY(result.dataInfo.version == std::string{"2026a"});
    QVERIFY(result.dataInfo.provenance == std::string{"IERS Rapid Service plus historical model"});
    QVERIFY(!result.dataInfo.diagnosticText.empty());

    const skygate::ephemeris::DeltaTEstimate estimate = result.provider->deltaTSeconds(epochForDate(2000, 1, 1));
    QVERIFY(estimate.isUsable());
    QCOMPARE(
        static_cast<std::uint8_t>(estimate.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTEstimateStatus::Available)
    );
    QVERIFY(estimate.deltaTSeconds.has_value());
    QVERIFY(std::abs(*estimate.deltaTSeconds - 63.83) < 0.001);
    QVERIFY(!estimate.diagnosticText.empty());
}

void DeltaTProviderTests::reportsMissingData()
{
    const TestEphemerisDataSnapshot snapshot(std::nullopt);
    const skygate::ephemeris::DeltaTDataLoadResult result = skygate::ephemeris::loadDeltaTDataFromSnapshot(snapshot);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTDataStatus::Missing)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void DeltaTProviderTests::rejectsMalformedRows()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version bad\n"
                    "effective_utc_date,delta_t_seconds\n"
                    "1900-01-01,-2.72\n"
                    "not-a-date,63.83\n";

    const skygate::ephemeris::DeltaTDataLoadResult result = skygate::ephemeris::loadDeltaTDataFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTDataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void DeltaTProviderTests::exposesAncientFallbackMetadata()
{
    const skygate::ephemeris::DeltaTDataLoadResult result =
        skygate::ephemeris::loadDeltaTDataFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.dataInfo.ancientFallbackModel.has_value());
    const skygate::ephemeris::DeltaTFallbackModelInfo& fallback = *result.dataInfo.ancientFallbackModel;
    QVERIFY(fallback.validityRange.id == std::string{"delta-t-ancient-fallback"});
    QVERIFY(fallback.provenance == std::string{"Morrison-Stephenson model"});
    QVERIFY(fallback.hasRepresentativeEstimate());
    QVERIFY(fallback.estimatedUncertaintySeconds.has_value());
    QCOMPARE(fallback.validityRange.start.julianDatePart1, epochForDate(-13200, 1, 1).julianDatePart1);
    QCOMPARE(fallback.validityRange.end.julianDatePart1, epochForDate(1600, 1, 1).julianDatePart1);

    const skygate::ephemeris::DeltaTEstimate estimate = result.provider->deltaTSeconds(epochForDate(-5000, 1, 1));
    QCOMPARE(
        static_cast<std::uint8_t>(estimate.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTEstimateStatus::Degraded)
    );
    QVERIFY(estimate.deltaTSeconds.has_value());
    QVERIFY(estimate.estimatedUncertaintySeconds.has_value());
    QVERIFY(!estimate.diagnosticText.empty());
}

void DeltaTProviderTests::exposesValidityRangeMetadata()
{
    const skygate::ephemeris::DeltaTDataLoadResult result =
        skygate::ephemeris::loadDeltaTDataFromTextAsset(makeValidAsset());

    QVERIFY(result.dataInfo.validityRange.has_value());
    QVERIFY(result.dataInfo.expiresAt.has_value());
    QVERIFY(result.dataInfo.validityRange->id == std::string{"delta-t"});
    QCOMPARE(result.dataInfo.validityRange->start.julianDatePart1, epochForDate(1900, 1, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.validityRange->end.julianDatePart1, epochForDate(2027, 1, 1).julianDatePart1);
}

void DeltaTProviderTests::reportsStaleData()
{
    skygate::ephemeris::DeltaTDataLoadOptions options;
    options.referenceEpoch = epochForDate(2030, 1, 1);

    const skygate::ephemeris::DeltaTDataLoadResult result =
        skygate::ephemeris::loadDeltaTDataFromTextAsset(makeValidAsset(), options);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::DeltaTDataStatus::Stale)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

QTEST_MAIN(DeltaTProviderTests)

#include "DeltaTProviderTests.moc"
