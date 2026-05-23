#include "time/CalendarTime.hpp"
#include "SkyContextControllerTestSupport.hpp"

#include "engine/highprecision/CalcephKernelProvider.hpp"

#include <cmath>
#include <filesystem>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

void compareEpoch(
    const skygate::ephemeris::AstronomicalEpoch& actual, const skygate::ephemeris::AstronomicalEpoch& expected
)
{
    QCOMPARE(actual.timeScale, expected.timeScale);
    QVERIFY(std::abs(actual.julianDatePart1 - expected.julianDatePart1) < 1.0e-12);
    QVERIFY(std::abs(actual.julianDatePart2 - expected.julianDatePart2) < 1.0e-12);
}

skygate::ephemeris::AstronomicalEpoch expectedUtcEpoch(
    const int astronomicalYear, const int month, const int day, const int hour, const int minute, const int second
)
{
    const auto epoch = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = astronomicalYear,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

SkySettingsStore::EphemerisUserSettingsSnapshot customEphemerisUserSettings()
{
    SkySettingsStore::EphemerisUserSettingsSnapshot settings;
    settings.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    settings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                               | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;
    settings.correctionPresetId = QStringLiteral("astrometric");
    settings.fallbackToSimpleEngine = false;
    settings.refractionEnabled = false;
    settings.atmosphericPressureHpa = 802.5;
    settings.atmosphericTemperatureC = -6.0;
    settings.relativeHumidity = 0.42;
    settings.observingWavelengthMicrometers = 0.68;
    settings.preferredDataProfileId = QStringLiteral("de441-long-range");
    settings.onlineUpdatesEnabled = false;
    settings.updatePresetId = QStringLiteral("custom");
    settings.updateManifestUrl = QStringLiteral("https://example.com/skygate-ephemeris.json");
    return settings;
}

bool writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

class TestCalcephKernelHandle final : public skygate::ephemeris::highprecision::ICalcephKernelHandle {
public:
    [[nodiscard]] std::optional<skygate::ephemeris::highprecision::SolarSystemKernelVector>
    computeGeometricState(const skygate::ephemeris::AstronomicalEpoch&, int, int) const override
    {
        return skygate::ephemeris::highprecision::SolarSystemKernelVector{.xAu = 1.0, .yAu = 0.0, .zAu = 0.0};
    }
};

class TestCalcephKernelRuntime final : public skygate::ephemeris::highprecision::ICalcephKernelRuntime {
public:
    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return true;
    }

    [[nodiscard]] skygate::ephemeris::highprecision::CalcephKernelOpenResult
    openKernel(const std::filesystem::path& path) const override
    {
        if (!QFileInfo::exists(QString::fromStdString(path.generic_string()))) {
            return {.diagnostic = "Test kernel file is missing."};
        }

        return {.handle = std::make_unique<TestCalcephKernelHandle>()};
    }
};

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 2000,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    const auto end = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "de440s-short-range",
        .displayName = "DE440sShortRange kernel",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifest makeInstalledEphemerisManifest(const QByteArray& kernelPayload)
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-de440s-short-range-data";
    manifest.dataSetInfo.displayName = "Test DE440s short-range data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = false,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.assets.push_back(
        skygate::ephemeris::EphemerisDataManifestAsset{
            .id = "de440s-kernel",
            .kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
            .profileId = "de440s-short-range",
            .version = "DE-test",
            .relativePath = "kernels/de440s.bsp",
            .checksum =
                skygate::ephemeris::EphemerisDataManifestChecksum{
                    .algorithm = "sha256",
                    .value = QCryptographicHash::hash(kernelPayload, QCryptographicHash::Sha256).toHex().toStdString(),
                },
            .compression =
                skygate::ephemeris::EphemerisDataManifestCompression{
                    .kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None,
                    .uncompressedSizeBytes = static_cast<std::uint64_t>(kernelPayload.size()),
                },
            .validityRange = testValidityRange(),
        }
    );
    return manifest;
}

SkySettingsStore::EphemerisDataCacheSnapshot installedEphemerisDataSnapshot(
    const QString& kernelPath,
    const QString& leapSecondPath,
    const QString& earthOrientationPath,
    const QString& deltaTPath
)
{
    SkySettingsStore::EphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    snapshot.installedKernelPath = kernelPath;
    snapshot.installedKernelVersion = QStringLiteral("DE-test");
    snapshot.installedLeapSecondTablePath = leapSecondPath;
    snapshot.installedLeapSecondTableVersion = QStringLiteral("LS-test");
    snapshot.installedEarthOrientationPath = earthOrientationPath;
    snapshot.installedEarthOrientationVersion = QStringLiteral("EOP-test");
    snapshot.installedDeltaTDataPath = deltaTPath;
    snapshot.installedDeltaTDataVersion = QStringLiteral("DT-test");
    snapshot.dataRevisionToken = QStringLiteral("installed-test");
    snapshot.lastUpdateResult = QStringLiteral("Installed test data");
    return snapshot;
}

void verifyEphemerisUserSettings(
    const SkySettingsStore::EphemerisUserSettingsSnapshot& actual,
    const SkySettingsStore::EphemerisUserSettingsSnapshot& expected
)
{
    QCOMPARE(static_cast<std::uint8_t>(actual.engineKind), static_cast<std::uint8_t>(expected.engineKind));
    QCOMPARE(static_cast<std::uint32_t>(actual.correctionFlags), static_cast<std::uint32_t>(expected.correctionFlags));
    QCOMPARE(actual.correctionPresetId, expected.correctionPresetId);
    QCOMPARE(actual.fallbackToSimpleEngine, expected.fallbackToSimpleEngine);
    QCOMPARE(actual.refractionEnabled, expected.refractionEnabled);
    QCOMPARE(actual.atmosphericPressureHpa, expected.atmosphericPressureHpa);
    QCOMPARE(actual.atmosphericTemperatureC, expected.atmosphericTemperatureC);
    QCOMPARE(actual.relativeHumidity, expected.relativeHumidity);
    QCOMPARE(actual.observingWavelengthMicrometers, expected.observingWavelengthMicrometers);
    QCOMPARE(actual.preferredDataProfileId, expected.preferredDataProfileId);
    QCOMPARE(actual.onlineUpdatesEnabled, expected.onlineUpdatesEnabled);
    QCOMPARE(actual.updatePresetId, expected.updatePresetId);
    QCOMPARE(actual.updateManifestUrl, expected.updateManifestUrl);
}

}  // namespace

class SkyContextControllerEphemerisSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void loadSavePreservesAllEphemerisUserSettings();
    void loadSettingsWithEphemerisSettingsNotifiesSceneConsumers();
    void loadSettingsBuildsHighPrecisionEngineFromInstalledEphemerisData();
    void requestContextUsesSimpleEngineDefaults();
    void requestContextCombinesRestoredSettingsObserverTimeAndDataRevision();
    void requestContextConvertsBceUtcToAstronomicalEpoch();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerEphemerisSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerEphemerisSettingsTests")));
}

void SkyContextControllerEphemerisSettingsTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerEphemerisSettingsTests::loadSavePreservesAllEphemerisUserSettings()
{
    const SkySettingsStore::EphemerisUserSettingsSnapshot expectedSettings = customEphemerisUserSettings();

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.ephemeris = expectedSettings;
    snapshot.ephemerisSettingsPresent = true;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    QVERIFY(controller->saveSettings());

    const auto savedSnapshot = store.loadState();
    QVERIFY(savedSnapshot.has_value());
    QVERIFY(savedSnapshot->ephemerisSettingsPresent);
    verifyEphemerisUserSettings(savedSnapshot->ephemeris, expectedSettings);
}

void SkyContextControllerEphemerisSettingsTests::loadSettingsBuildsHighPrecisionEngineFromInstalledEphemerisData()
{
    const QByteArray kernelPayload("test kernel payload");
    const QString kernelPath = m_settings.currentTestFilePath(QStringLiteral("de440s.bsp"));
    const QString leapSecondPath = m_settings.currentTestFilePath(QStringLiteral("leap-seconds.list"));
    const QString earthOrientationPath = m_settings.currentTestFilePath(QStringLiteral("eop.txt"));
    const QString deltaTPath = m_settings.currentTestFilePath(QStringLiteral("deltat.data"));
    QVERIFY(writeFile(kernelPath, kernelPayload));
    QVERIFY(writeFile(
        leapSecondPath,
        QByteArrayLiteral(
            "# IANA leap-second file sample\n"
            "# File expires on 28 December 2026\n"
            "#@ 4007404800\n"
            "#NTP Time      DTAI    Day Month Year\n"
            "2272060800     10      # 1 Jan 1972\n"
            "3692217600     37      # 1 Jan 2017\n"
        )
    ));
    QVERIFY(writeFile(
        earthOrientationPath,
        QByteArrayLiteral(
            "EARTH ORIENTATION PARAMETER (EOP) PRODUCT CENTER CENTER (PARIS OBSERVATORY)\n"
            "Date      MJD      x          y        UT1-UTC       LOD\n"
            "(0h UTC)\n"
            "2026  5  1  60431   0.112300   0.218700   0.0314200   0.001723\n"
            "2026  5  2  60432   0.118000   0.221000   0.0345000   0.001669\n"
        )
    ));
    QVERIFY(writeFile(
        deltaTPath,
        QByteArrayLiteral(
            "1900  1  1  -2.7200\n"
            "2000  1  1  63.8300\n"
            "2026  1  1  69.2000\n"
        )
    ));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        installedEphemerisDataSnapshot(kernelPath, leapSecondPath, earthOrientationPath, deltaTPath)
    ));
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.ephemerisSettingsPresent = true;
    snapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    snapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    snapshot.ephemeris.refractionEnabled = true;
    QVERIFY(store.saveState(snapshot));

    const skygate::ephemeris::EphemerisDataManifest manifest = makeInstalledEphemerisManifest(kernelPayload);
    SkyContextController::InitializationOptions options = controllerInitializationOptions(true);
    options.ephemerisFactoryInputs.dataManifest = &manifest;
    options.ephemerisFactoryInputs.calcephKernelRuntime = std::make_shared<TestCalcephKernelRuntime>();
    const auto controller = createControllerWithOptions(options);

    QVERIFY(controller->ephemerisEngine() != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));
}

void SkyContextControllerEphemerisSettingsTests::loadSettingsWithEphemerisSettingsNotifiesSceneConsumers()
{
    const auto controller = createController(false);
    SkySettingsStore store;
    QVERIFY(controller->saveSettings());
    auto snapshot = store.loadState();
    QVERIFY(snapshot.has_value());
    snapshot->ephemeris = customEphemerisUserSettings();
    snapshot->ephemerisSettingsPresent = true;
    QVERIFY(store.saveState(*snapshot));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);

    QVERIFY(controller->loadSettings());

    QVERIFY(skyContextChangedSpy.count() >= 1);
    QVERIFY(controller->ephemerisEngine() != nullptr);
    const auto restoredOptions = controller->ephemerisEngine()->options();
    QCOMPARE(
        static_cast<std::uint32_t>(restoredOptions.correctionFlags),
        static_cast<std::uint32_t>(snapshot->ephemeris.correctionFlags)
    );
    QCOMPARE(restoredOptions.fallbackToSimpleEngine, snapshot->ephemeris.fallbackToSimpleEngine);
    QCOMPARE(restoredOptions.enableAtmosphericRefraction, snapshot->ephemeris.refractionEnabled);
}

void SkyContextControllerEphemerisSettingsTests::requestContextUsesSimpleEngineDefaults()
{
    const FakeTimeSource timeSource(QDateTime(QDate(2026, 5, 14), QTime(8, 45, 30), QTimeZone::UTC));
    const auto controller = createControllerWithTimeSource(timeSource, false);

    const auto requestContext = controller->ephemerisRequestContext();

    QCOMPARE(requestContext.request.context.utcTime, controller->skyContext().utcTime);
    QCOMPARE(requestContext.request.context.observer.latitudeDeg, controller->skyContext().observer.latitudeDeg);
    QCOMPARE(
        static_cast<std::uint8_t>(requestContext.request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections)
    );
    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(2026, 5, 14, 8, 45, 30));
    QVERIFY(requestContext.activeDataSnapshot != nullptr);
    QCOMPARE(requestContext.ephemerisDataRevision, controller->ephemerisDataRevision());
    QCOMPARE(requestContext.catalogRevision, controller->catalogRevision());
}

void SkyContextControllerEphemerisSettingsTests::requestContextCombinesRestoredSettingsObserverTimeAndDataRevision()
{
    auto expectedSettings = customEphemerisUserSettings();
    expectedSettings.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    expectedSettings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent
                                       | skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction;
    expectedSettings.refractionEnabled = true;

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.ephemeris = expectedSettings;
    snapshot.ephemerisSettingsPresent = true;
    snapshot.utcEpochMicros = QDateTime(QDate(2030, 7, 2), QTime(3, 4, 5), QTimeZone::UTC).toMSecsSinceEpoch() * 1000;
    snapshot.live = false;
    snapshot.latitudeDeg = 47.3769;
    snapshot.longitudeDeg = 8.5417;
    snapshot.elevationMeters = 408.0;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    const auto requestContext = controller->ephemerisRequestContext();

    QCOMPARE(
        static_cast<std::uint8_t>(requestContext.request.options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags),
        static_cast<std::uint32_t>(expectedSettings.correctionFlags)
    );
    QCOMPARE(requestContext.request.options.enableAtmosphericRefraction, expectedSettings.refractionEnabled);
    QCOMPARE(requestContext.request.options.atmosphericPressureHpa, expectedSettings.atmosphericPressureHpa);
    QCOMPARE(requestContext.request.options.atmosphericTemperatureC, expectedSettings.atmosphericTemperatureC);
    QCOMPARE(requestContext.request.options.relativeHumidity, expectedSettings.relativeHumidity);
    QCOMPARE(
        requestContext.request.options.observingWavelengthMicrometers, expectedSettings.observingWavelengthMicrometers
    );
    QCOMPARE(requestContext.request.context.observer.latitudeDeg, snapshot.latitudeDeg);
    QCOMPARE(requestContext.request.context.observer.longitudeDeg, snapshot.longitudeDeg);
    QCOMPARE(requestContext.request.context.observer.elevationMeters, snapshot.elevationMeters);
    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(2030, 7, 2, 3, 4, 5));
    QVERIFY(requestContext.activeDataSnapshot != nullptr);
    QCOMPARE(requestContext.ephemerisDataRevision, controller->ephemerisDataRevision());
}

void SkyContextControllerEphemerisSettingsTests::requestContextConvertsBceUtcToAstronomicalEpoch()
{
    const auto controller = createController(false);
    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    const auto requestContext = controller->ephemerisRequestContext();

    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(-43, 3, 15, 12, 0, 0));
}

QTEST_GUILESS_MAIN(SkyContextControllerEphemerisSettingsTests)

#include "SkyContextControllerEphemerisSettingsTests.moc"
