#include "time/CalendarTime.hpp"
#include "engine/highprecision/DeltaTDataLoader.hpp"

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
    const auto epoch = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
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

class DeltaTProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsPresentDataFromSnapshot();
    void loadsUsnoDeltaTData();
    void reportsMissingData();
    void rejectsMalformedRows();
    void exposesAncientFallbackMetadata();
    void exposesValidityRangeMetadata();
    void returnsUnavailableBetweenLastTableRowAndExpiration();
    void rejectsAncientFallbackWithoutRepresentativeEstimate();
    void rejectsPartialAncientFallbackRanges();
    void reportsStaleData();
};

void DeltaTProviderTests::loadsPresentDataFromSnapshot()
{
    const TestEphemerisDataSnapshot snapshot(makeValidAsset());
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromSnapshot(snapshot);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Available)
    );
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QVERIFY(result.dataInfo.version == std::string{"2026a"});
    QVERIFY(result.dataInfo.provenance == std::string{"IERS Rapid Service plus historical model"});
    QVERIFY(!result.dataInfo.diagnosticText.empty());

    const skygate::ephemeris::IDeltaTProvider::Estimate estimate =
        result.provider->deltaTSeconds(epochForDate(2000, 1, 1));
    QVERIFY(estimate.isUsable());
    QCOMPARE(
        static_cast<std::uint8_t>(estimate.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::EstimateStatus::Available)
    );
    QVERIFY(estimate.deltaTSeconds.has_value());
    QVERIFY(std::abs(*estimate.deltaTSeconds - 63.83) < 0.001);
    QVERIFY(!estimate.diagnosticText.empty());
}

void DeltaTProviderTests::loadsUsnoDeltaTData()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "1973  2  1  43.4724\n"
                    "1973  3  1  43.5648\n"
                    "2026  1  1  69.2000\n";

    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.astronomicalYear, 1973);
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.month, 2);
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.day, 1);
    const skygate::ephemeris::IDeltaTProvider::Estimate estimate =
        result.provider->deltaTSeconds(epochForDate(2026, 1, 1));
    QVERIFY(estimate.isUsable());
    QVERIFY(estimate.deltaTSeconds.has_value());
    QVERIFY(std::abs(*estimate.deltaTSeconds - 69.2) < 0.001);
}

void DeltaTProviderTests::reportsMissingData()
{
    const TestEphemerisDataSnapshot snapshot(std::nullopt);
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromSnapshot(snapshot);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Missing)
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

    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void DeltaTProviderTests::exposesAncientFallbackMetadata()
{
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.dataInfo.ancientFallbackModel.has_value());
    const skygate::ephemeris::IDeltaTProvider::DataInfo::FallbackModelInfo& fallback =
        *result.dataInfo.ancientFallbackModel;
    QVERIFY(fallback.validityRange.id == std::string{"delta-t-ancient-fallback"});
    QVERIFY(fallback.provenance == std::string{"Morrison-Stephenson model"});
    QVERIFY(fallback.hasRepresentativeEstimate());
    QCOMPARE(*fallback.representativeDeltaTSeconds, 12000.5);
    QVERIFY(fallback.estimatedUncertaintySeconds.has_value());
    QCOMPARE(*fallback.estimatedUncertaintySeconds, 7200.0);
    QCOMPARE(fallback.validityRange.start.julianDatePart1, epochForDate(-13200, 1, 1).julianDatePart1);
    QCOMPARE(fallback.validityRange.start.julianDatePart2, epochForDate(-13200, 1, 1).julianDatePart2);
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.validityRange.start.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Utc)
    );
    QCOMPARE(fallback.validityRange.end.julianDatePart1, epochForDate(1600, 1, 1).julianDatePart1);
    QCOMPARE(fallback.validityRange.end.julianDatePart2, epochForDate(1600, 1, 1).julianDatePart2);
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.validityRange.end.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Utc)
    );

    const skygate::ephemeris::IDeltaTProvider::Estimate estimate =
        result.provider->deltaTSeconds(epochForDate(-5000, 1, 1));
    QCOMPARE(
        static_cast<std::uint8_t>(estimate.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::EstimateStatus::Degraded)
    );
    QVERIFY(estimate.deltaTSeconds.has_value());
    QVERIFY(estimate.estimatedUncertaintySeconds.has_value());
    QVERIFY(!estimate.diagnosticText.empty());
}

void DeltaTProviderTests::exposesValidityRangeMetadata()
{
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(makeValidAsset());

    QVERIFY(result.dataInfo.validityRange.has_value());
    QVERIFY(result.dataInfo.expiresAt.has_value());
    QVERIFY(result.dataInfo.validityRange->id == std::string{"delta-t"});
    QCOMPARE(result.dataInfo.validityRange->start.julianDatePart1, epochForDate(1900, 1, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.validityRange->start.julianDatePart2, epochForDate(1900, 1, 1).julianDatePart2);
    QCOMPARE(result.dataInfo.validityRange->end.julianDatePart1, epochForDate(2026, 1, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.validityRange->end.julianDatePart2, epochForDate(2026, 1, 1).julianDatePart2);
    QCOMPARE(result.dataInfo.expiresAt->julianDatePart1, epochForDate(2027, 1, 1).julianDatePart1);
    QCOMPARE(result.dataInfo.expiresAt->julianDatePart2, epochForDate(2027, 1, 1).julianDatePart2);
}

void DeltaTProviderTests::returnsUnavailableBetweenLastTableRowAndExpiration()
{
    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(makeValidAsset());

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);

    const skygate::ephemeris::IDeltaTProvider::Estimate estimate =
        result.provider->deltaTSeconds(epochForDate(2026, 6, 1));
    QVERIFY(!estimate.isUsable());
    QCOMPARE(
        static_cast<std::uint8_t>(estimate.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::EstimateStatus::Unavailable)
    );
    QVERIFY(!estimate.deltaTSeconds.has_value());
    QVERIFY(!estimate.diagnosticText.empty());
}

void DeltaTProviderTests::rejectsAncientFallbackWithoutRepresentativeEstimate()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version bad-fallback\n"
                    "#@ ancient_fallback_start -13200-01-01\n"
                    "#@ ancient_fallback_end 1600-01-01\n"
                    "#@ ancient_fallback_source Historical model\n"
                    "effective_utc_date,delta_t_seconds\n"
                    "1900-01-01,-2.72\n"
                    "2000-01-01,63.83\n";

    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Malformed)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

void DeltaTProviderTests::rejectsPartialAncientFallbackRanges()
{
    skygate::ephemeris::EphemerisTextDataAsset startOnlyAsset = makeValidAsset();
    startOnlyAsset.content = "#@ version start-only-fallback\n"
                             "#@ ancient_fallback_start -13200-01-01\n"
                             "#@ ancient_fallback_delta_t_seconds 12000.5\n"
                             "effective_utc_date,delta_t_seconds\n"
                             "1900-01-01,-2.72\n"
                             "2000-01-01,63.83\n";

    const skygate::ephemeris::DeltaTDataLoader::Result startOnlyResult =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(startOnlyAsset);

    QVERIFY(!startOnlyResult.isSuccess());
    QVERIFY(startOnlyResult.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(startOnlyResult.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Malformed)
    );

    skygate::ephemeris::EphemerisTextDataAsset endOnlyAsset = makeValidAsset();
    endOnlyAsset.content = "#@ version end-only-fallback\n"
                           "#@ ancient_fallback_end 1600-01-01\n"
                           "#@ ancient_fallback_delta_t_seconds 12000.5\n"
                           "effective_utc_date,delta_t_seconds\n"
                           "1900-01-01,-2.72\n"
                           "2000-01-01,63.83\n";

    const skygate::ephemeris::DeltaTDataLoader::Result endOnlyResult =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(endOnlyAsset);

    QVERIFY(!endOnlyResult.isSuccess());
    QVERIFY(endOnlyResult.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(endOnlyResult.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Malformed)
    );
}

void DeltaTProviderTests::reportsStaleData()
{
    skygate::ephemeris::DeltaTDataLoader::Options options;
    options.referenceEpoch = epochForDate(2030, 1, 1);

    const skygate::ephemeris::DeltaTDataLoader::Result result =
        skygate::ephemeris::DeltaTDataLoader::loadFromTextAsset(makeValidAsset(), options);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.dataInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::IDeltaTProvider::DataStatus::Stale)
    );
    QVERIFY(!result.dataInfo.diagnosticText.empty());
}

QTEST_MAIN(DeltaTProviderTests)

#include "DeltaTProviderTests.moc"
