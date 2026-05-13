#include "skygate/ephemeris/Types.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

class EphemerisApiModelTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesExactlyTwoEngineKinds();
    void constructsHighPrecisionModelDefaults();
    void combinesCorrectionFlags();
    void constructsRequestAndDataSetModels();
};

void EphemerisApiModelTests::exposesExactlyTwoEngineKinds()
{
    constexpr std::array<skygate::ephemeris::EphemerisEngineKind, skygate::ephemeris::ephemerisEngineKindCount()>
        engineKinds{
            skygate::ephemeris::EphemerisEngineKind::Simple,
            skygate::ephemeris::EphemerisEngineKind::HighPrecision,
        };

    QCOMPARE(engineKinds.size(), 2U);
    QVERIFY(skygate::ephemeris::displayName(engineKinds[0]) == "Simple");
    QVERIFY(skygate::ephemeris::displayName(engineKinds[1]) == "High precision");
}

void EphemerisApiModelTests::constructsHighPrecisionModelDefaults()
{
    skygate::ephemeris::EphemerisEngineOptions options;
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(options.fallbackToSimpleEngine);
    QVERIFY(options.enableAtmosphericRefraction);
    QVERIFY(options.atmosphericPressureHpa > 0.0);
    QVERIFY(options.observingWavelengthMicrometers > 0.0);
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));

    skygate::ephemeris::EphemerisCapabilities capabilities;
    QCOMPARE(
        static_cast<std::uint8_t>(capabilities.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(capabilities.supportedCorrections),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::None)
    );
    QVERIFY(!capabilities.supportsSolarSystemBodies);
    QVERIFY(!capabilities.supportsExtendedHistoricalRange);
}

void EphemerisApiModelTests::combinesCorrectionFlags()
{
    auto flags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                 | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;
    flags |= skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax;

    QVERIFY(skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime));
    QVERIFY(
        skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration)
    );
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(flags, skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax)
    );
    QVERIFY(!skygate::ephemeris::hasCorrectionFlag(
        flags, skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));

    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::Astrometric,
        skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    ));
    QVERIFY(!skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::Astrometric,
        skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax
    ));
    QVERIFY(skygate::ephemeris::hasCorrectionFlag(
        skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric,
        skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
    ));
}

void EphemerisApiModelTests::constructsRequestAndDataSetModels()
{
    skygate::ephemeris::AstronomicalEpoch epoch;
    epoch.julianDatePart1 = 2'460'310.0;
    epoch.julianDatePart2 = 0.5;
    epoch.timeScale = skygate::ephemeris::TimeScale::Tt;

    skygate::ephemeris::EphemerisRequest request;
    request.epoch = epoch;
    request.context.observer.latitudeDeg = 37.7749;
    request.context.observer.longitudeDeg = -122.4194;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;

    QCOMPARE(
        static_cast<std::uint8_t>(request.epoch.timeScale), static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tt)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(request.context.utcTime.time_since_epoch().count(), 1'704'067'200);

    skygate::ephemeris::EphemerisDateRange modernRange;
    modernRange.id = "modern";
    modernRange.displayName = "Modern kernel";
    modernRange.start = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'300'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };
    modernRange.end = skygate::ephemeris::AstronomicalEpoch{
        .julianDatePart1 = 2'600'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
    };

    skygate::ephemeris::EphemerisDataSetInfo dataSet;
    dataSet.id = "de440s";
    dataSet.displayName = "DE440s";
    dataSet.version = "test";
    dataSet.provenance = "JPL";
    dataSet.dateRanges.push_back(modernRange);

    QCOMPARE(dataSet.dateRanges.size(), std::size_t{1});
    QVERIFY(dataSet.dateRanges.front().id == std::string("modern"));
    QCOMPARE(
        static_cast<std::uint8_t>(dataSet.dateRanges.front().start.timeScale),
        static_cast<std::uint8_t>(skygate::ephemeris::TimeScale::Tdb)
    );
}

static_assert(std::is_enum_v<skygate::ephemeris::EphemerisEngineKind>);
static_assert(std::is_enum_v<skygate::ephemeris::EphemerisCorrectionFlags>);

QTEST_MAIN(EphemerisApiModelTests)

#include "EphemerisApiModelTests.moc"
