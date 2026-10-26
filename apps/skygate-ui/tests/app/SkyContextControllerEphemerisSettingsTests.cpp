#include "StaticCalcephKernelProvider.hpp"
#include "TestCalcephKernel.hpp"
#include "ephemeris/SkyEphemerisSettingsPolicy.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/SkyContextControllerTestSupport.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"
#include "support/SkyContextTimeTestSupport.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

using skygate::ui::tests::setControllerUtcDateTime;

namespace {

void compareEpoch(const skygate::core::AstronomicalEpoch& actual, const skygate::core::AstronomicalEpoch& expected)
{
    QCOMPARE(actual.timeScale, expected.timeScale);
    QVERIFY(std::abs(actual.julianDatePart1 - expected.julianDatePart1) < 1.0e-12);
    QVERIFY(std::abs(actual.julianDatePart2 - expected.julianDatePart2) < 1.0e-12);
}

skygate::core::AstronomicalEpoch expectedUtcEpoch(
    const int astronomicalYear, const int month, const int day, const int hour, const int minute, const int second
)
{
    const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = astronomicalYear,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

SkyEphemerisUserSettingsSnapshot customEphemerisUserSettings()
{
    SkyEphemerisUserSettingsSnapshot settings;
    settings.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    settings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
                               | skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration();
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

bool writeInstalledSupportFiles(
    const QString& leapSecondPath, const QString& earthOrientationPath, const QString& deltaTPath
)
{
    return writeFile(
               leapSecondPath,
               QByteArrayLiteral(
                   "# IANA leap-second file sample\n"
                   "# File expires on 28 December 2026\n"
                   "#@ 4007404800\n"
                   "#NTP Time      DTAI    Day Month Year\n"
                   "2272060800     10      # 1 Jan 1972\n"
                   "3692217600     37      # 1 Jan 2017\n"
               )
           )
           && writeFile(
               earthOrientationPath,
               QByteArrayLiteral(
                   "EARTH ORIENTATION PARAMETER (EOP) PRODUCT CENTER CENTER (PARIS OBSERVATORY)\n"
                   "Date      MJD      x          y        UT1-UTC       LOD\n"
                   "(0h UTC)\n"
                   "2026  5  1  60431   0.112300   0.218700   0.0314200   0.001723\n"
                   "2026  5  2  60432   0.118000   0.221000   0.0345000   0.001669\n"
               )
           )
           && writeFile(
               deltaTPath,
               QByteArrayLiteral(
                   "1900  1  1  -2.7200\n"
                   "2000  1  1  63.8300\n"
                   "2026  1  1  69.2000\n"
               )
           );
}

skygate::ephemeris::EphemerisDateRange testValidityRange();

[[nodiscard]] std::shared_ptr<skygate::ephemeris::highprecision::ICalcephKernelProvider> makeTestCalcephKernelProvider()
{
    auto kernel = std::make_shared<skygate::ephemeris::tests::TestCalcephKernel>(
        skygate::ephemeris::highprecision::ICalcephKernel::Info{
            .id = "de440s-kernel",
            .profileId = "modern",
            .version = "installed-test",
            .provenance = "Installed test data",
            .validityRange = testValidityRange(),
        }
    );
    skygate::ephemeris::highprecision::ICalcephKernel::StateResult result;
    result.positionAu = skygate::core::Vector3d{.x = 1.0, .y = 0.0, .z = 0.0};
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    kernel->setDefaultResult(std::move(result));
    return std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(std::move(kernel));
}

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2000,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    const auto end = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
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
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = false,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.assets.push_back(
        skygate::ephemeris::EphemerisDataManifest::Asset{
            .id = "de440s-kernel",
            .kind = skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel,
            .profileId = "de440s-short-range",
            .version = "DE-test",
            .relativePath = "kernels/de440s.bsp",
            .checksum =
                skygate::ephemeris::EphemerisDataManifest::Checksum{
                    .algorithm = "sha256",
                    .value = QCryptographicHash::hash(kernelPayload, QCryptographicHash::Sha256).toHex().toStdString(),
                },
            .compression =
                skygate::ephemeris::EphemerisDataManifest::Compression{
                    .kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None,
                    .uncompressedSizeBytes = static_cast<std::uint64_t>(kernelPayload.size()),
                },
            .validityRange = testValidityRange(),
        }
    );
    return manifest;
}

SkyEphemerisDataCacheSnapshot installedEphemerisDataSnapshot(
    const QString& kernelPath,
    const QString& leapSecondPath,
    const QString& earthOrientationPath,
    const QString& deltaTPath
)
{
    SkyEphemerisDataCacheSnapshot snapshot;
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
    const SkyEphemerisUserSettingsSnapshot& actual, const SkyEphemerisUserSettingsSnapshot& expected
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

void expectRejectedAtmosphereText(
    SkyContextController& controller,
    QSignalSpy& ephemerisSettingsSpy,
    QSignalSpy& skyContextSpy,
    const std::uint64_t expectedRevision,
    void (SkyContextController::*setter)(const QString& text),
    QString (SkyContextController::*getter)() const,
    const QString& text
)
{
    const QString textBefore = (controller.*getter)();
    const int settingsCountBefore = ephemerisSettingsSpy.count();
    const int skyContextCountBefore = skyContextSpy.count();

    (controller.*setter)(text);

    QCOMPARE((controller.*getter)(), textBefore);
    QCOMPARE(ephemerisSettingsSpy.count(), settingsCountBefore + 1);
    QCOMPARE(skyContextSpy.count(), skyContextCountBefore);
    QCOMPARE(controller.ephemerisRequestContext().engineOptionsRevision, expectedRevision);
}

}  // namespace

class SkyContextControllerEphemerisSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void loadSavePreservesAllEphemerisUserSettings();
    void loadSettingsNotifiesSceneConsumers();
    void loadSettingsBuildsHighPrecisionEngineFromData();
    void cachedProvidersRebuildAndInvalidate();
    void requestContextUsesSimpleEngineDefaults();
    void requestContextCombinesObserverTimeAndRevision();
    void requestContextConvertsBceUtcToAstronomicalEpoch();
    void ephemerisEngineKindIndexRejectsOutOfRangeValues();
    void engineSwitchResetsCorrectionPresetToDefaults();
    void oldHandleStaysUsableAfterReplacement();
    void engineModelExposesRegistryLabelsAndCapabilities();
    void correctionPresetMappingsMatchEngineFlags();
    void invalidCorrectionPresetKeepsOptionsAndNotifies();
    void invalidEngineKindKeepsOptionsAndNotifies();
    void atmosphereTextBoundsAcceptEdges();
    void invalidAtmosphereTextKeepsOptionsAndNotifies();
    void acceptedSettingsNotifyAndBumpRevisionOnce();
    void unchangedOptionSettersKeepRevision();
    void engineSwitchKeepsUnrelatedSettings();
    void savedSnapshotMirrorsAppliedEngineOptions();

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
    const SkyEphemerisUserSettingsSnapshot expectedSettings = customEphemerisUserSettings();

    SkySettingsStore store;
    SkyPersistedStateSnapshot snapshot;
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

void SkyContextControllerEphemerisSettingsTests::loadSettingsBuildsHighPrecisionEngineFromData()
{
    const QByteArray kernelPayload("test kernel payload");
    const QString kernelPath = m_settings.currentTestFilePath(QStringLiteral("de440s.bsp"));
    const QString leapSecondPath = m_settings.currentTestFilePath(QStringLiteral("leap-seconds.list"));
    const QString earthOrientationPath = m_settings.currentTestFilePath(QStringLiteral("eop.txt"));
    const QString deltaTPath = m_settings.currentTestFilePath(QStringLiteral("deltat.data"));
    QVERIFY(writeFile(kernelPath, kernelPayload));
    QVERIFY(writeInstalledSupportFiles(leapSecondPath, earthOrientationPath, deltaTPath));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        installedEphemerisDataSnapshot(kernelPath, leapSecondPath, earthOrientationPath, deltaTPath)
    ));
    SkyPersistedStateSnapshot snapshot;
    snapshot.ephemerisSettingsPresent = true;
    snapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    snapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric();
    snapshot.ephemeris.refractionEnabled = true;
    QVERIFY(store.saveState(snapshot));

    const skygate::ephemeris::EphemerisDataManifest manifest = makeInstalledEphemerisManifest(kernelPayload);
    SkyContextController::InitializationOptions options = controllerInitializationOptions(true);
    options.ephemerisFactoryInputs.dataManifest = &manifest;
    options.ephemerisFactoryInputs.calcephKernelProvider = makeTestCalcephKernelProvider();
    const auto controller = createControllerWithOptions(options);

    QVERIFY(controller->ephemerisEngine() != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));
}

void SkyContextControllerEphemerisSettingsTests::cachedProvidersRebuildAndInvalidate()
{
    QFile fixtureKernel(QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/kernels/de432s.bsp"));
    QVERIFY(fixtureKernel.open(QIODevice::ReadOnly));
    const QByteArray kernelPayload = fixtureKernel.readAll();

    const QString kernelPath = m_settings.currentTestFilePath(QStringLiteral("de432s.bsp"));
    const QString leapSecondPath = m_settings.currentTestFilePath(QStringLiteral("leap-seconds.list"));
    const QString earthOrientationPath = m_settings.currentTestFilePath(QStringLiteral("eop.txt"));
    const QString deltaTPath = m_settings.currentTestFilePath(QStringLiteral("deltat.data"));
    QVERIFY(writeFile(kernelPath, kernelPayload));
    QVERIFY(writeInstalledSupportFiles(leapSecondPath, earthOrientationPath, deltaTPath));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        installedEphemerisDataSnapshot(kernelPath, leapSecondPath, earthOrientationPath, deltaTPath)
    ));
    SkyPersistedStateSnapshot snapshot;
    snapshot.ephemerisSettingsPresent = true;
    snapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    snapshot.ephemeris.fallbackToSimpleEngine = true;
    QVERIFY(store.saveState(snapshot));

    const skygate::ephemeris::EphemerisDataManifest manifest = makeInstalledEphemerisManifest(kernelPayload);
    SkyContextController::InitializationOptions options = controllerInitializationOptions(true);
    options.ephemerisFactoryInputs.dataManifest = &manifest;
#if !defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    // The simple backend build has no CALCEPH runtime, so the real installed
    // kernel cannot be opened. Inject the established fake kernel provider to
    // exercise the cached-provider rebuild/invalidation contract
    // deterministically. The high-precision build keeps the real
    // installed-kernel integration path below.
    options.ephemerisFactoryInputs.calcephKernelProvider = makeTestCalcephKernelProvider();
#endif
    const auto controller = createControllerWithOptions(options);

    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );

    // A rebuild without a data change reuses the cached providers, so the
    // high-precision engine is still created.
    controller->setEphemerisRefractionEnabled(true);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );

    // A data change drops the cached providers. Without the invalidation the
    // stale time-scale and kernel providers would keep the high-precision
    // engine alive even though the support data is gone.
    controller->setEphemerisEngineKindIndex(0);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(controller->clearSupportDataCache());
    controller->setEphemerisEngineKindIndex(1);
    QCOMPARE(
        static_cast<std::uint8_t>(controller->ephemerisEngine()->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
}

void SkyContextControllerEphemerisSettingsTests::loadSettingsNotifiesSceneConsumers()
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
        static_cast<std::uint32_t>(restoredOptions.correctionFlags()),
        static_cast<std::uint32_t>(snapshot->ephemeris.correctionFlags)
    );
    QCOMPARE(restoredOptions.fallbackToSimpleEngine(), snapshot->ephemeris.fallbackToSimpleEngine);
    QCOMPARE(restoredOptions.enableAtmosphericRefraction(), snapshot->ephemeris.refractionEnabled);
}

void SkyContextControllerEphemerisSettingsTests::requestContextUsesSimpleEngineDefaults()
{
    const FakeTimeSource timeSource(QDateTime(QDate(2026, 5, 14), QTime(8, 45, 30), QTimeZone::UTC));
    const auto controller = createControllerWithTimeSource(timeSource, false);

    const auto requestContext = controller->ephemerisRequestContext();

    QCOMPARE(requestContext.request.context.utcTime, controller->skyContext().utcTime);
    QCOMPARE(requestContext.request.context.observer.latitudeDeg, controller->skyContext().observer.latitudeDeg);
    QCOMPARE(
        static_cast<std::uint8_t>(requestContext.request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections())
    );
    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(2026, 5, 14, 8, 45, 30));
    QVERIFY(requestContext.activeDataSnapshot != nullptr);
    QCOMPARE(requestContext.ephemerisDataRevision, controller->ephemerisDataRevision());
    QCOMPARE(requestContext.catalogRevision, controller->catalogRevision());
}

void SkyContextControllerEphemerisSettingsTests::requestContextCombinesObserverTimeAndRevision()
{
    auto expectedSettings = customEphemerisUserSettings();
    expectedSettings.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    expectedSettings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::apparent()
                                       | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction();
    expectedSettings.refractionEnabled = true;

    SkySettingsStore store;
    SkyPersistedStateSnapshot snapshot;
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
        static_cast<std::uint8_t>(requestContext.request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(requestContext.request.options.correctionFlags()),
        static_cast<std::uint32_t>(expectedSettings.correctionFlags)
    );
    QCOMPARE(requestContext.request.options.enableAtmosphericRefraction(), expectedSettings.refractionEnabled);
    QCOMPARE(requestContext.request.options.atmosphericPressureHpa(), expectedSettings.atmosphericPressureHpa);
    QCOMPARE(requestContext.request.options.atmosphericTemperatureC(), expectedSettings.atmosphericTemperatureC);
    QCOMPARE(requestContext.request.options.relativeHumidity(), expectedSettings.relativeHumidity);
    QCOMPARE(
        requestContext.request.options.observingWavelengthMicrometers(), expectedSettings.observingWavelengthMicrometers
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
    QVERIFY(setControllerUtcDateTime(*controller, "0044-03-15 BCE", "12:00:00"));

    const auto requestContext = controller->ephemerisRequestContext();

    compareEpoch(requestContext.request.epoch, expectedUtcEpoch(-43, 3, 15, 12, 0, 0));
}

void SkyContextControllerEphemerisSettingsTests::ephemerisEngineKindIndexRejectsOutOfRangeValues()
{
    const auto controller = createController(false);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);

    controller->setEphemerisEngineKindIndex(-1);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);

    controller->setEphemerisEngineKindIndex(99);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
}

void SkyContextControllerEphemerisSettingsTests::engineSwitchResetsCorrectionPresetToDefaults()
{
    const auto controller = createController(false);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);

    controller->setEphemerisEngineKindIndex(1);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 3);
    QVERIFY(controller->ephemerisRefractionEnabled());

    controller->setEphemerisCorrectionPresetIndex(1);
    controller->setEphemerisRefractionEnabled(false);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 1);
    QVERIFY(!controller->ephemerisRefractionEnabled());

    controller->setEphemerisEngineKindIndex(0);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 0);
    QVERIFY(!controller->ephemerisRefractionEnabled());

    controller->setEphemerisEngineKindIndex(1);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 3);
    QVERIFY(controller->ephemerisRefractionEnabled());
}

void SkyContextControllerEphemerisSettingsTests::oldHandleStaysUsableAfterReplacement()
{
    const auto controller = createController(false);
    const std::shared_ptr<const skygate::ephemeris::IEphemerisEngine> retainedHandle =
        controller->ephemerisEngineHandle();
    QVERIFY(retainedHandle != nullptr);
    QVERIFY(controller->ephemerisEngine() == retainedHandle.get());

    // Switching to the high-precision engine without providers falls back to a
    // simple engine, which replaces the retained one.
    controller->setEphemerisEngineKindIndex(1);

    QVERIFY(controller->ephemerisEngine() != retainedHandle.get());
    const skygate::ephemeris::EphemerisSnapshot snapshot = retainedHandle->compute(controller->skyContext());
    QVERIFY(!snapshot.states.empty());
}

void SkyContextControllerEphemerisSettingsTests::engineModelExposesRegistryLabelsAndCapabilities()
{
    const auto controller = createController(false);

    const QStringList labels = controller->ephemerisEngineLabels();
    QCOMPARE(labels.size(), 2);
    QCOMPARE(labels.at(0), QString("Simple"));
    QCOMPARE(labels.at(1), QString("High precision"));

    QVERIFY(!controller->ephemerisEngineSupportsCorrections(0));
    QVERIFY(!controller->ephemerisEngineSupportsAtmosphereSettings(0));
    QVERIFY(controller->ephemerisEngineSupportsCorrections(1));
    QVERIFY(controller->ephemerisEngineSupportsAtmosphereSettings(1));

    QCOMPARE(controller->ephemerisEngineDefaultCorrectionPresetIndex(0), 0);
    QVERIFY(!controller->ephemerisEngineDefaultRefractionEnabled(0));
    QCOMPARE(controller->ephemerisEngineDefaultCorrectionPresetIndex(1), 3);
    QVERIFY(controller->ephemerisEngineDefaultRefractionEnabled(1));
    QCOMPARE(controller->ephemerisEngineDefaultCorrectionPresetIndex(-1), 0);
    QVERIFY(!controller->ephemerisEngineDefaultRefractionEnabled(-1));
    QCOMPARE(controller->ephemerisEngineDefaultCorrectionPresetIndex(99), 0);
    QVERIFY(!controller->ephemerisEngineDefaultRefractionEnabled(99));
}

void SkyContextControllerEphemerisSettingsTests::correctionPresetMappingsMatchEngineFlags()
{
    const auto controller = createController(false);
    controller->setEphemerisEngineKindIndex(1);
    controller->setEphemerisRefractionEnabled(false);

    const std::array<std::pair<int, skygate::ephemeris::EphemerisCorrectionFlags>, 4> presets{{
        {0, skygate::ephemeris::EphemerisCorrectionFlags::geometric()},
        {1, skygate::ephemeris::EphemerisCorrectionFlags::astrometric()},
        {2, skygate::ephemeris::EphemerisCorrectionFlags::apparent()},
        {3, skygate::ephemeris::EphemerisCorrectionFlags::topocentric()},
    }};
    for (const auto& [presetIndex, expectedFlags] : presets) {
        controller->setEphemerisCorrectionPresetIndex(presetIndex);
        QCOMPARE(controller->ephemerisCorrectionPresetIndex(), presetIndex);
        QCOMPARE(controller->ephemerisEngine()->options().correctionFlags(), expectedFlags);
    }

    controller->setEphemerisRefractionEnabled(true);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 3);
    QCOMPARE(
        controller->ephemerisEngine()->options().correctionFlags(),
        skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric()
    );

    controller->setEphemerisCorrectionPresetIndex(2);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 2);
    QCOMPARE(
        controller->ephemerisEngine()->options().correctionFlags(),
        skygate::ephemeris::EphemerisCorrectionFlags::apparent()
            | skygate::ephemeris::EphemerisCorrectionFlags::atmosphericRefraction()
    );

    controller->setEphemerisRefractionEnabled(false);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 2);
    QCOMPARE(
        controller->ephemerisEngine()->options().correctionFlags(),
        skygate::ephemeris::EphemerisCorrectionFlags::apparent()
    );
}

void SkyContextControllerEphemerisSettingsTests::invalidCorrectionPresetKeepsOptionsAndNotifies()
{
    const auto controller = createController(false);
    controller->setEphemerisEngineKindIndex(1);
    controller->setEphemerisCorrectionPresetIndex(1);
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    const skygate::ephemeris::EphemerisCorrectionFlags flags =
        controller->ephemerisEngine()->options().correctionFlags();
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);

    controller->setEphemerisCorrectionPresetIndex(-1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 1);
    QCOMPARE(controller->ephemerisEngine()->options().correctionFlags(), flags);
    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision);
    QCOMPARE(ephemerisSettingsSpy.count(), 1);
    QCOMPARE(skyContextSpy.count(), 0);

    controller->setEphemerisCorrectionPresetIndex(4);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 1);
    QCOMPARE(controller->ephemerisEngine()->options().correctionFlags(), flags);
    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision);
    QCOMPARE(ephemerisSettingsSpy.count(), 2);
    QCOMPARE(skyContextSpy.count(), 0);
}

void SkyContextControllerEphemerisSettingsTests::invalidEngineKindKeepsOptionsAndNotifies()
{
    const auto controller = createController(false);
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("900"));
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);

    controller->setEphemerisEngineKindIndex(-1);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("900.00"));
    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision);
    QCOMPARE(ephemerisSettingsSpy.count(), 1);
    QCOMPARE(skyContextSpy.count(), 0);

    controller->setEphemerisEngineKindIndex(99);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("900.00"));
    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision);
    QCOMPARE(ephemerisSettingsSpy.count(), 2);
    QCOMPARE(skyContextSpy.count(), 0);
}

void SkyContextControllerEphemerisSettingsTests::atmosphereTextBoundsAcceptEdges()
{
    const auto controller = createController(false);
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);

    controller->setEphemerisAtmosphericPressureText(QStringLiteral("0"));
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("0.00"));
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("2000"));
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("2000.00"));

    controller->setEphemerisAtmosphericTemperatureText(QStringLiteral("-100"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("-100.0"));
    controller->setEphemerisAtmosphericTemperatureText(QStringLiteral("80"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("80.0"));

    controller->setEphemerisRelativeHumidityText(QStringLiteral("100"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("100.0"));
    controller->setEphemerisRelativeHumidityText(QStringLiteral("0"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("0.0"));

    controller->setEphemerisWavelengthText(QStringLiteral("0.1"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("0.10"));
    controller->setEphemerisWavelengthText(QStringLiteral("100"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("100.00"));

    controller->setEphemerisAtmosphericPressureText(QStringLiteral(" 900.50 "));
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("900.50"));

    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision + 9);
    QCOMPARE(ephemerisSettingsSpy.count(), 9);
    QCOMPARE(skyContextSpy.count(), 9);
}

void SkyContextControllerEphemerisSettingsTests::invalidAtmosphereTextKeepsOptionsAndNotifies()
{
    const auto controller = createController(false);
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("900"));
    controller->setEphemerisAtmosphericTemperatureText(QStringLiteral("5"));
    controller->setEphemerisRelativeHumidityText(QStringLiteral("40"));
    controller->setEphemerisWavelengthText(QStringLiteral("0.62"));
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);

    const auto pressureSetter = &SkyContextController::setEphemerisAtmosphericPressureText;
    const auto pressureGetter = &SkyContextController::ephemerisAtmosphericPressureText;
    const auto temperatureSetter = &SkyContextController::setEphemerisAtmosphericTemperatureText;
    const auto temperatureGetter = &SkyContextController::ephemerisAtmosphericTemperatureText;
    const auto humiditySetter = &SkyContextController::setEphemerisRelativeHumidityText;
    const auto humidityGetter = &SkyContextController::ephemerisRelativeHumidityText;
    const auto wavelengthSetter = &SkyContextController::setEphemerisWavelengthText;
    const auto wavelengthGetter = &SkyContextController::ephemerisWavelengthText;

    for (const QString& text : {
             QStringLiteral("-0.01"),
             QStringLiteral("2000.01"),
             QStringLiteral("not a number"),
             QString(),
         }) {
        expectRejectedAtmosphereText(
            *controller, ephemerisSettingsSpy, skyContextSpy, revision, pressureSetter, pressureGetter, text
        );
    }
    for (const QString& text : {
             QStringLiteral("-100.01"),
             QStringLiteral("80.01"),
             QStringLiteral("not a number"),
             QString(),
         }) {
        expectRejectedAtmosphereText(
            *controller, ephemerisSettingsSpy, skyContextSpy, revision, temperatureSetter, temperatureGetter, text
        );
    }
    for (const QString& text : {
             QStringLiteral("-0.01"),
             QStringLiteral("100.01"),
             QStringLiteral("not a number"),
             QString(),
         }) {
        expectRejectedAtmosphereText(
            *controller, ephemerisSettingsSpy, skyContextSpy, revision, humiditySetter, humidityGetter, text
        );
    }
    for (const QString& text : {
             QStringLiteral("0.09"),
             QStringLiteral("100.01"),
             QStringLiteral("not a number"),
             QString(),
         }) {
        expectRejectedAtmosphereText(
            *controller, ephemerisSettingsSpy, skyContextSpy, revision, wavelengthSetter, wavelengthGetter, text
        );
    }

    QCOMPARE(ephemerisSettingsSpy.count(), 16);
    QCOMPARE(skyContextSpy.count(), 0);
}

void SkyContextControllerEphemerisSettingsTests::acceptedSettingsNotifyAndBumpRevisionOnce()
{
    const auto controller = createController(false);
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);
    QStringList notifications;
    connect(controller.get(), &SkyContextController::ephemerisSettingsChanged, controller.get(), [&notifications] {
        notifications.append(QStringLiteral("settings"));
    });
    connect(controller.get(), &SkyContextController::skyContextChanged, controller.get(), [&notifications] {
        notifications.append(QStringLiteral("skyContext"));
    });

    controller->setEphemerisAtmosphericPressureText(QStringLiteral("900"));

    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision + 1);
    QCOMPARE(ephemerisSettingsSpy.count(), 1);
    QCOMPARE(skyContextSpy.count(), 1);
    QCOMPARE(notifications, QStringList({QStringLiteral("settings"), QStringLiteral("skyContext")}));
    QCOMPARE(controller->ephemerisRequestContext().request.options.atmosphericPressureHpa(), 900.0);

    notifications.clear();
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("2000.01"));

    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision + 1);
    QCOMPARE(ephemerisSettingsSpy.count(), 2);
    QCOMPARE(skyContextSpy.count(), 1);
    QCOMPARE(notifications, QStringList({QStringLiteral("settings")}));
}

void SkyContextControllerEphemerisSettingsTests::unchangedOptionSettersKeepRevision()
{
    const auto controller = createController(false);
    controller->setEphemerisEngineKindIndex(1);
    controller->setEphemerisCorrectionPresetIndex(1);
    controller->setEphemerisRefractionEnabled(true);
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("900.00"));
    controller->setEphemerisAtmosphericTemperatureText(QStringLiteral("5.0"));
    controller->setEphemerisRelativeHumidityText(QStringLiteral("42.0"));
    controller->setEphemerisWavelengthText(QStringLiteral("0.62"));
    const std::uint64_t revision = controller->ephemerisRequestContext().engineOptionsRevision;
    QSignalSpy ephemerisSettingsSpy(controller.get(), &SkyContextController::ephemerisSettingsChanged);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);

    controller->setEphemerisEngineKindIndex(controller->ephemerisEngineKindIndex());
    controller->setEphemerisCorrectionPresetIndex(controller->ephemerisCorrectionPresetIndex());
    controller->setEphemerisRefractionEnabled(controller->ephemerisRefractionEnabled());
    controller->setEphemerisAtmosphericPressureText(controller->ephemerisAtmosphericPressureText());
    controller->setEphemerisAtmosphericTemperatureText(controller->ephemerisAtmosphericTemperatureText());
    controller->setEphemerisRelativeHumidityText(controller->ephemerisRelativeHumidityText());
    controller->setEphemerisWavelengthText(controller->ephemerisWavelengthText());

    QCOMPARE(controller->ephemerisRequestContext().engineOptionsRevision, revision);
    QCOMPARE(ephemerisSettingsSpy.count(), 0);
    QCOMPARE(skyContextSpy.count(), 0);
}

void SkyContextControllerEphemerisSettingsTests::engineSwitchKeepsUnrelatedSettings()
{
    SkySettingsStore store;
    SkyPersistedStateSnapshot snapshot;
    snapshot.ephemeris = customEphemerisUserSettings();
    snapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::astrometric();
    snapshot.ephemerisSettingsPresent = true;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 1);
    QVERIFY(!controller->ephemerisRefractionEnabled());

    controller->setEphemerisEngineKindIndex(1);

    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 3);
    QVERIFY(controller->ephemerisRefractionEnabled());
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("802.50"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("-6.0"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("42.0"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("0.68"));
    QVERIFY(controller->saveSettings());
    auto savedSnapshot = store.loadState();
    QVERIFY(savedSnapshot.has_value());
    QVERIFY(!savedSnapshot->ephemeris.fallbackToSimpleEngine);
    QCOMPARE(savedSnapshot->ephemeris.correctionPresetId, QString("apparent-topocentric"));
    QCOMPARE(
        savedSnapshot->ephemeris.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric()
    );

    controller->setEphemerisCorrectionPresetIndex(1);
    controller->setEphemerisRefractionEnabled(false);
    controller->setEphemerisEngineKindIndex(0);

    QCOMPARE(controller->ephemerisEngineKindIndex(), 0);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 0);
    QVERIFY(!controller->ephemerisRefractionEnabled());
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("802.50"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("-6.0"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("42.0"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("0.68"));
    QVERIFY(controller->saveSettings());
    savedSnapshot = store.loadState();
    QVERIFY(savedSnapshot.has_value());
    QVERIFY(!savedSnapshot->ephemeris.fallbackToSimpleEngine);
    QCOMPARE(savedSnapshot->ephemeris.correctionPresetId, QString("geometric"));
    QCOMPARE(savedSnapshot->ephemeris.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());

    controller->setEphemerisEngineKindIndex(1);

    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 3);
    QVERIFY(controller->ephemerisRefractionEnabled());
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("802.50"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("-6.0"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("42.0"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("0.68"));
}

void SkyContextControllerEphemerisSettingsTests::savedSnapshotMirrorsAppliedEngineOptions()
{
    const auto controller = createController(false);
    controller->setEphemerisEngineKindIndex(1);
    controller->setEphemerisCorrectionPresetIndex(2);
    controller->setEphemerisRefractionEnabled(false);
    controller->setEphemerisAtmosphericPressureText(QStringLiteral("802.50"));
    controller->setEphemerisAtmosphericTemperatureText(QStringLiteral("-6.0"));
    controller->setEphemerisRelativeHumidityText(QStringLiteral("42.0"));
    controller->setEphemerisWavelengthText(QStringLiteral("0.68"));
    QVERIFY(controller->saveSettings());

    SkySettingsStore store;
    const auto savedSnapshot = store.loadState();
    QVERIFY(savedSnapshot.has_value());
    QVERIFY(savedSnapshot->ephemerisSettingsPresent);

    SkyEphemerisUserSettingsSnapshot expectedSettings;
    expectedSettings.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    expectedSettings.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::apparent();
    expectedSettings.correctionPresetId = QStringLiteral("apparent");
    expectedSettings.refractionEnabled = false;
    expectedSettings.atmosphericPressureHpa = 802.5;
    expectedSettings.atmosphericTemperatureC = -6.0;
    expectedSettings.relativeHumidity = 0.42;
    expectedSettings.observingWavelengthMicrometers = 0.68;
    expectedSettings.updateManifestUrl =
        skygate::ui::internal::SkyEphemerisSettingsPolicy::kDefaultUpdateManifestUrl.toString();
    verifyEphemerisUserSettings(savedSnapshot->ephemeris, expectedSettings);
}

QTEST_GUILESS_MAIN(SkyContextControllerEphemerisSettingsTests)

#include "SkyContextControllerEphemerisSettingsTests.moc"
