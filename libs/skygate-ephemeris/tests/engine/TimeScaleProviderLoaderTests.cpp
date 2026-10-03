#include "time/CalendarTime.hpp"
#include "engine/EphemerisTextDataAsset.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/TimeScaleProviderLoader.hpp"

#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    TestEphemerisDataSnapshot(
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTable,
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationData,
        std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTData
    )
        : m_leapSecondTable(std::move(leapSecondTable)), m_earthOrientationData(std::move(earthOrientationData)),
          m_deltaTData(std::move(deltaTData))
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return m_leapSecondTable;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> earthOrientationDataAsset() const override
    {
        return m_earthOrientationData;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> deltaTDataAsset() const override
    {
        return m_deltaTData;
    }

private:
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_leapSecondTable;
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_earthOrientationData;
    std::optional<skygate::ephemeris::EphemerisTextDataAsset> m_deltaTData;
};

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeAsset(std::string id, std::string content)
{
    return {
        .id = std::move(id),
        .version = "test-version",
        .provenance = "test",
        .content = std::move(content),
    };
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidLeapSecondTable()
{
    return makeAsset(
        "leap-seconds",
        "# IANA leap-second file sample\n"
        "# File expires on 28 December 2026\n"
        "#@ 4007404800\n"
        "#NTP Time      DTAI    Day Month Year\n"
        "2272060800     10      # 1 Jan 1972\n"
        "3692217600     37      # 1 Jan 2017\n"
    );
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidEarthOrientationData()
{
    return makeAsset(
        "earth-orientation",
        "EARTH ORIENTATION PARAMETER (EOP) PRODUCT CENTER CENTER (PARIS OBSERVATORY)\n"
        "Date      MJD      x          y        UT1-UTC       LOD\n"
        "(0h UTC)\n"
        "2026  5  1  60431   0.112300   0.218700   0.0314200   0.001723\n"
        "2026  5  2  60432   0.118000   0.221000   0.0345000   0.001669\n"
    );
}

[[nodiscard]] skygate::ephemeris::EphemerisTextDataAsset makeValidDeltaTData()
{
    return makeAsset(
        "delta-t",
        "1900  1  1  -2.7200\n"
        "2000  1  1  63.8300\n"
        "2026  1  1  69.2000\n"
    );
}

[[nodiscard]] std::optional<skygate::core::AstronomicalEpoch>
epochForDate(const int year, const int month, const int day)
{
    return skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .hour = 12,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
}

}  // namespace

class TimeScaleProviderLoaderTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsProvidersFromValidSnapshot();
    void rejectsMissingLeapSecondTable();
    void rejectsInvalidEarthOrientationData();
    void usesProvidedEarthOrientationProvider();
    void toleratesMissingDeltaTData();
};

void TimeScaleProviderLoaderTests::loadsProvidersFromValidSnapshot()
{
    TestEphemerisDataSnapshot snapshot(
        makeValidLeapSecondTable(), makeValidEarthOrientationData(), makeValidDeltaTData()
    );
    const auto earthOrientationProvider =
        skygate::ephemeris::TimeScaleProviderLoader::loadEarthOrientationProvider(snapshot);
    QVERIFY(earthOrientationProvider != nullptr);

    const auto timeScaleService =
        skygate::ephemeris::TimeScaleProviderLoader::loadTimeScaleService(snapshot, earthOrientationProvider);
    QVERIFY(timeScaleService != nullptr);

    const auto epoch = epochForDate(2026, 5, 1);
    QVERIFY(epoch.has_value());
    const skygate::ephemeris::TimeScaleConversionResult conversion =
        timeScaleService->convert(*epoch, skygate::core::TimeScale::Tt);
    QVERIFY(conversion.isSuccess());
    QCOMPARE(conversion.epoch.timeScale, skygate::core::TimeScale::Tt);
}

void TimeScaleProviderLoaderTests::rejectsMissingLeapSecondTable()
{
    TestEphemerisDataSnapshot snapshot(std::nullopt, makeValidEarthOrientationData(), makeValidDeltaTData());
    const auto earthOrientationProvider =
        skygate::ephemeris::TimeScaleProviderLoader::loadEarthOrientationProvider(snapshot);
    QVERIFY(earthOrientationProvider != nullptr);

    const auto timeScaleService =
        skygate::ephemeris::TimeScaleProviderLoader::loadTimeScaleService(snapshot, earthOrientationProvider);
    QVERIFY(timeScaleService == nullptr);
}

void TimeScaleProviderLoaderTests::rejectsInvalidEarthOrientationData()
{
    TestEphemerisDataSnapshot snapshot(
        makeValidLeapSecondTable(), makeAsset("earth-orientation", "not an EOP product\n"), makeValidDeltaTData()
    );
    const auto earthOrientationProvider =
        skygate::ephemeris::TimeScaleProviderLoader::loadEarthOrientationProvider(snapshot);
    QVERIFY(earthOrientationProvider == nullptr);

    const auto timeScaleService =
        skygate::ephemeris::TimeScaleProviderLoader::loadTimeScaleService(snapshot, earthOrientationProvider);
    QVERIFY(timeScaleService != nullptr);
}

void TimeScaleProviderLoaderTests::usesProvidedEarthOrientationProvider()
{
    TestEphemerisDataSnapshot snapshot(
        makeValidLeapSecondTable(), makeValidEarthOrientationData(), makeValidDeltaTData()
    );
    const auto providedEarthOrientationProvider =
        skygate::ephemeris::TimeScaleProviderLoader::loadEarthOrientationProvider(snapshot);
    QVERIFY(providedEarthOrientationProvider != nullptr);

    const auto timeScaleService =
        skygate::ephemeris::TimeScaleProviderLoader::loadTimeScaleService(snapshot, providedEarthOrientationProvider);
    QVERIFY(timeScaleService != nullptr);

    const auto epoch = epochForDate(2026, 5, 1);
    QVERIFY(epoch.has_value());
    const skygate::ephemeris::TimeScaleConversionResult conversion =
        timeScaleService->convert(*epoch, skygate::core::TimeScale::Ut1);
    QVERIFY(conversion.isSuccess());
    QCOMPARE(conversion.epoch.timeScale, skygate::core::TimeScale::Ut1);
}

void TimeScaleProviderLoaderTests::toleratesMissingDeltaTData()
{
    TestEphemerisDataSnapshot snapshot(makeValidLeapSecondTable(), makeValidEarthOrientationData(), std::nullopt);
    const auto earthOrientationProvider =
        skygate::ephemeris::TimeScaleProviderLoader::loadEarthOrientationProvider(snapshot);
    QVERIFY(earthOrientationProvider != nullptr);

    const auto timeScaleService =
        skygate::ephemeris::TimeScaleProviderLoader::loadTimeScaleService(snapshot, earthOrientationProvider);
    QVERIFY(timeScaleService != nullptr);

    const auto epoch = epochForDate(2026, 5, 1);
    QVERIFY(epoch.has_value());
    const skygate::ephemeris::TimeScaleConversionResult conversion =
        timeScaleService->convert(*epoch, skygate::core::TimeScale::Tt);
    QVERIFY(conversion.isSuccess());
    QCOMPARE(conversion.epoch.timeScale, skygate::core::TimeScale::Tt);
}

QTEST_APPLESS_MAIN(TimeScaleProviderLoaderTests)
#include "TimeScaleProviderLoaderTests.moc"
