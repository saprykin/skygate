#include "skygate/ephemeris/LeapSecondProvider.hpp"

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
        return m_asset;
    }

private:
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_asset;
};

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidAsset()
{
    return {
        .id = "leap-seconds",
        .version = "asset-version",
        .provenance = "bundled",
        .content = "#@ version 2026a\n"
                   "#@ source IERS Bulletin C\n"
                   "#@ expires 2027-01-01\n"
                   "effective_utc_date,tai_minus_utc\n"
                   "1972-01-01,10\n"
                   "1972-07-01,11\n"
                   "2017-01-01,37\n",
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

class LeapSecondProviderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsValidTableFromSnapshot();
    void loadsIanaLeapSecondList();
    void reportsMissingTable();
    void rejectsMalformedRows();
    void rejectsMalformedExpirationMetadata();
    void reportsStaleTable();
    void exposesValidityRangeMetadata();
};

void LeapSecondProviderTests::loadsValidTableFromSnapshot()
{
    const TestEphemerisDataSnapshot snapshot(makeValidAsset());
    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromSnapshot(snapshot);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.tableInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::LeapSecondTableStatus::Available)
    );
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QVERIFY(result.tableInfo.version == std::string{"2026a"});
    QVERIFY(result.tableInfo.provenance == std::string{"IERS Bulletin C"});
    QVERIFY(!result.tableInfo.diagnosticText.empty());

    QCOMPARE(result.provider->taiMinusUtcSeconds(epochForDate(1972, 3, 1)).value_or(-1), 10);
    QCOMPARE(result.provider->taiMinusUtcSeconds(epochForDate(2018, 1, 1)).value_or(-1), 37);
    QVERIFY(!result.provider->taiMinusUtcSeconds(epochForDate(1971, 12, 31)).has_value());
}

void LeapSecondProviderTests::loadsIanaLeapSecondList()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "# IANA leap-second file sample\n"
                    "# File expires on 28 December 2026\n"
                    "#@\t4007404800\n"
                    "#NTP Time      DTAI    Day Month Year\n"
                    "2272060800     10      # 1 Jan 1972\n"
                    "2287785600     11      # 1 Jul 1972\n"
                    "3692217600     37      # 1 Jan 2017\n";

    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromTextAsset(asset);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(result.provider->entries().size(), std::size_t{3});
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.astronomicalYear, 1972);
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.month, 1);
    QCOMPARE(result.provider->entries().front().effectiveUtcDate.day, 1);
    QCOMPARE(result.provider->taiMinusUtcSeconds(epochForDate(2018, 1, 1)).value_or(-1), 37);
    QVERIFY(result.tableInfo.expiresAt.has_value());
    QCOMPARE(result.tableInfo.expiresAt->julianDatePart1, epochForDate(2026, 12, 28).julianDatePart1);
}

void LeapSecondProviderTests::reportsMissingTable()
{
    const TestEphemerisDataSnapshot snapshot(std::nullopt);
    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromSnapshot(snapshot);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.tableInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::LeapSecondTableStatus::Missing)
    );
    QVERIFY(!result.tableInfo.diagnosticText.empty());
}

void LeapSecondProviderTests::rejectsMalformedRows()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version bad\n"
                    "effective_utc_date,tai_minus_utc\n"
                    "1972-01-01,10\n"
                    "not-a-date,11\n";

    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.tableInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::LeapSecondTableStatus::Malformed)
    );
    QVERIFY(!result.tableInfo.diagnosticText.empty());
}

void LeapSecondProviderTests::rejectsMalformedExpirationMetadata()
{
    skygate::ephemeris::EphemerisTextDataAsset asset = makeValidAsset();
    asset.content = "#@ version bad-expiration\n"
                    "#@ expires not-a-date\n"
                    "effective_utc_date,tai_minus_utc\n"
                    "1972-01-01,10\n";

    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromTextAsset(asset);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.provider == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.tableInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::LeapSecondTableStatus::Malformed)
    );
    QVERIFY(!result.tableInfo.expiresAt.has_value());
    QVERIFY(!result.tableInfo.validityRange.has_value());
    QVERIFY(!result.tableInfo.diagnosticText.empty());
}

void LeapSecondProviderTests::reportsStaleTable()
{
    skygate::ephemeris::LeapSecondTableLoadOptions options;
    options.referenceEpoch = epochForDate(2030, 1, 1);

    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromTextAsset(makeValidAsset(), options);

    QVERIFY(result.isSuccess());
    QVERIFY(result.provider != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(result.tableInfo.status),
        static_cast<std::uint8_t>(skygate::ephemeris::LeapSecondTableStatus::Stale)
    );
    QVERIFY(!result.tableInfo.diagnosticText.empty());
    QCOMPARE(result.provider->taiMinusUtcSeconds(epochForDate(2018, 1, 1)).value_or(-1), 37);
}

void LeapSecondProviderTests::exposesValidityRangeMetadata()
{
    const skygate::ephemeris::LeapSecondTableLoadResult result =
        skygate::ephemeris::loadLeapSecondTableFromTextAsset(makeValidAsset());

    QVERIFY(result.tableInfo.validityRange.has_value());
    QVERIFY(result.tableInfo.expiresAt.has_value());
    QVERIFY(result.tableInfo.validityRange->id == std::string{"leap-seconds"});
    QCOMPARE(result.tableInfo.validityRange->start.julianDatePart1, epochForDate(1972, 1, 1).julianDatePart1);
    QCOMPARE(result.tableInfo.validityRange->end.julianDatePart1, epochForDate(2027, 1, 1).julianDatePart1);
}

QTEST_MAIN(LeapSecondProviderTests)

#include "LeapSecondProviderTests.moc"
