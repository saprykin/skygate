#include "diagnostics/SkyLogging.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstdint>

namespace {

void ignoreSkySettingsFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid .*setting skyContext/.* - using fallback .*"));
    }
}

}  // namespace

class SkySettingsStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesAndLoadsStateSnapshot();
    void savesAndLoadsNegativeUtcEpochMicros();
    void loadsLegacyUtcEpochSecondsAsMicros();
    void savesLoadsAndDefaultsMainWindowSize();
    void defaultsRoundTripThroughStore();
    void malformedStateValuesFallBackToDefaults();
    void partialStateAndUnknownOverlayKeysAreTolerated();
    void malformedEphemerisUserSettingsFallBackToDefaults();
    void savesLoadsAndDefaultsLoggingPreferences();
    void malformedLoggingPreferencesFallBackToDefaults();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkySettingsStoreTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkygateUiSettingsStoreTests")));
}

void SkySettingsStoreTests::savesAndLoadsStateSnapshot()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkyPersistedStateSnapshot savedSnapshot;
    savedSnapshot.live = false;
    savedSnapshot.timelineToolbarCollapsed = true;
    savedSnapshot.searchToolbarCollapsed = true;
    savedSnapshot.speedMultiplier = 4.0;
    savedSnapshot.stepSeconds = 300;
    savedSnapshot.magnitudeCutoff = 7.5;
    savedSnapshot.viewCenterAltitudeDeg = 18.0;
    savedSnapshot.viewCenterAzimuthDeg = 220.0;
    savedSnapshot.viewFieldOfViewDeg = 74.0;
    savedSnapshot.utcEpochMicros = 1'717'276'800'123'000LL;
    savedSnapshot.latitudeDeg = 47.0;
    savedSnapshot.longitudeDeg = 8.0;
    savedSnapshot.elevationMeters = 409.0;
    savedSnapshot.locationSourceText = "City";
    savedSnapshot.selectedCityId = "ch-zurich";
    savedSnapshot.displayTimeZoneId = "Europe/Zurich";
    savedSnapshot.projectionTypeText = "Perspective";
    savedSnapshot.themeId = "night-vision";
    savedSnapshot.overlayLayers.horizon = false;
    savedSnapshot.overlayLayers.altAzGrid = false;
    savedSnapshot.overlayLayers.constellationLines = false;
    savedSnapshot.overlayLayers.constellationLabels = false;
    savedSnapshot.overlayLayers.ecliptic = true;
    savedSnapshot.overlayLayers.celestialEquator = true;
    savedSnapshot.overlayLayers.circumpolarBoundary = true;
    savedSnapshot.overlayLayers.solarSystemLabels = false;
    savedSnapshot.overlayLayers.deepSkyObjects = false;
    savedSnapshot.overlayLayers.deepSkyLabels = false;
    savedSnapshot.catalogPresetIndex = 2;
    savedSnapshot.catalogUrlText = "https://example.com/catalog.csv";
    savedSnapshot.deepSkyCatalogPresetIndex = 1;
    savedSnapshot.deepSkyCatalogUrlText = "https://example.com/NGC.csv";
    savedSnapshot.logToTerminal = false;
    savedSnapshot.logToFile = true;
    savedSnapshot.logFilePath = m_settings.filePath(QStringLiteral("skygate-test.log"));
    savedSnapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    savedSnapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
                                              | skygate::ephemeris::EphemerisCorrectionFlags::stellarAberration();
    savedSnapshot.ephemeris.correctionPresetId = QStringLiteral("astrometric");
    savedSnapshot.ephemeris.fallbackToSimpleEngine = false;
    savedSnapshot.ephemeris.refractionEnabled = false;
    savedSnapshot.ephemeris.atmosphericPressureHpa = 810.0;
    savedSnapshot.ephemeris.atmosphericTemperatureC = -2.5;
    savedSnapshot.ephemeris.relativeHumidity = 0.62;
    savedSnapshot.ephemeris.observingWavelengthMicrometers = 0.7;
    savedSnapshot.ephemeris.preferredDataProfileId = QStringLiteral("de441-long-range");
    savedSnapshot.ephemeris.onlineUpdatesEnabled = false;
    savedSnapshot.ephemeris.updatePresetId = QStringLiteral("custom");
    savedSnapshot.ephemeris.updateManifestUrl = QStringLiteral("https://example.com/ephemeris.json");
    savedSnapshot.ephemerisSettingsPresent = true;

    QVERIFY(store.saveState(savedSnapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->live, savedSnapshot.live);
    QCOMPARE(loadedSnapshot->timelineToolbarCollapsed, savedSnapshot.timelineToolbarCollapsed);
    QCOMPARE(loadedSnapshot->searchToolbarCollapsed, savedSnapshot.searchToolbarCollapsed);
    QCOMPARE(loadedSnapshot->speedMultiplier, savedSnapshot.speedMultiplier);
    QCOMPARE(loadedSnapshot->stepSeconds, savedSnapshot.stepSeconds);
    QCOMPARE(loadedSnapshot->utcEpochMicros, savedSnapshot.utcEpochMicros);
    QCOMPARE(loadedSnapshot->locationSourceText, savedSnapshot.locationSourceText);
    QCOMPARE(loadedSnapshot->selectedCityId, savedSnapshot.selectedCityId);
    QCOMPARE(loadedSnapshot->displayTimeZoneId, savedSnapshot.displayTimeZoneId);
    QCOMPARE(loadedSnapshot->projectionTypeText, savedSnapshot.projectionTypeText);
    QCOMPARE(loadedSnapshot->themeId, savedSnapshot.themeId);
    QVERIFY(loadedSnapshot->overlayLayers.equals(savedSnapshot.overlayLayers));
    QCOMPARE(loadedSnapshot->catalogPresetIndex, savedSnapshot.catalogPresetIndex);
    QCOMPARE(loadedSnapshot->catalogUrlText, savedSnapshot.catalogUrlText);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPresetIndex, savedSnapshot.deepSkyCatalogPresetIndex);
    QCOMPARE(loadedSnapshot->deepSkyCatalogUrlText, savedSnapshot.deepSkyCatalogUrlText);
    QCOMPARE(loadedSnapshot->logToTerminal, savedSnapshot.logToTerminal);
    QCOMPARE(loadedSnapshot->logToFile, savedSnapshot.logToFile);
    QCOMPARE(loadedSnapshot->logFilePath, savedSnapshot.logFilePath);
    QCOMPARE(
        static_cast<std::uint8_t>(loadedSnapshot->ephemeris.engineKind),
        static_cast<std::uint8_t>(savedSnapshot.ephemeris.engineKind)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(loadedSnapshot->ephemeris.correctionFlags),
        static_cast<std::uint32_t>(savedSnapshot.ephemeris.correctionFlags)
    );
    QCOMPARE(loadedSnapshot->ephemeris.correctionPresetId, savedSnapshot.ephemeris.correctionPresetId);
    QCOMPARE(loadedSnapshot->ephemeris.fallbackToSimpleEngine, savedSnapshot.ephemeris.fallbackToSimpleEngine);
    QCOMPARE(loadedSnapshot->ephemeris.refractionEnabled, savedSnapshot.ephemeris.refractionEnabled);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericPressureHpa, savedSnapshot.ephemeris.atmosphericPressureHpa);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericTemperatureC, savedSnapshot.ephemeris.atmosphericTemperatureC);
    QCOMPARE(loadedSnapshot->ephemeris.relativeHumidity, savedSnapshot.ephemeris.relativeHumidity);
    QCOMPARE(
        loadedSnapshot->ephemeris.observingWavelengthMicrometers, savedSnapshot.ephemeris.observingWavelengthMicrometers
    );
    QCOMPARE(loadedSnapshot->ephemeris.preferredDataProfileId, savedSnapshot.ephemeris.preferredDataProfileId);
    QCOMPARE(loadedSnapshot->ephemeris.onlineUpdatesEnabled, savedSnapshot.ephemeris.onlineUpdatesEnabled);
    QCOMPARE(loadedSnapshot->ephemeris.updatePresetId, savedSnapshot.ephemeris.updatePresetId);
    QCOMPARE(loadedSnapshot->ephemeris.updateManifestUrl, savedSnapshot.ephemeris.updateManifestUrl);
    QCOMPARE(loadedSnapshot->ephemerisSettingsPresent, true);
}

void SkySettingsStoreTests::savesAndLoadsNegativeUtcEpochMicros()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkyPersistedStateSnapshot savedSnapshot;
    savedSnapshot.utcEpochMicros = -63'555'595'200'123'000LL;

    QVERIFY(store.saveState(savedSnapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->utcEpochMicros, savedSnapshot.utcEpochMicros);
}

void SkySettingsStoreTests::loadsLegacyUtcEpochSecondsAsMicros()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/utcEpochSeconds", -1234LL);

    const SkySettingsStore store;
    const auto loadedSnapshot = store.loadState();

    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->utcEpochMicros, -1'234'000'000LL);
}

void SkySettingsStoreTests::savesLoadsAndDefaultsMainWindowSize()
{
    QSettings settings;
    settings.clear();

    const SkySettingsStore store;
    QCOMPARE(store.loadMainWindowSize(), QSize(1100, 760));

    QVERIFY(store.saveMainWindowSize(QSize(1280, 820)));
    QCOMPARE(store.loadMainWindowSize(), QSize(1280, 820));

    settings.setValue("app/mainWindowWidth", "wide");
    settings.setValue("app/mainWindowHeight", 12);
    QCOMPARE(store.loadMainWindowSize(), QSize(1100, 760));

    QVERIFY(store.saveMainWindowSize(QSize(300, 300)));
    QCOMPARE(store.loadMainWindowSize(), QSize(1100, 760));
}

void SkySettingsStoreTests::defaultsRoundTripThroughStore()
{
    QSettings settings;
    settings.clear();

    const SkySettingsStore store;
    const SkyPersistedStateSnapshot defaults;
    QVERIFY(store.saveState(defaults));

    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->stepSeconds, defaults.stepSeconds);
    QCOMPARE(loadedSnapshot->magnitudeCutoff, defaults.magnitudeCutoff);
    QCOMPARE(loadedSnapshot->viewFieldOfViewDeg, defaults.viewFieldOfViewDeg);
    QCOMPARE(loadedSnapshot->viewCenterAltitudeDeg, defaults.viewCenterAltitudeDeg);
    QCOMPARE(loadedSnapshot->viewCenterAzimuthDeg, defaults.viewCenterAzimuthDeg);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericPressureHpa, defaults.ephemeris.atmosphericPressureHpa);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericTemperatureC, defaults.ephemeris.atmosphericTemperatureC);
    QCOMPARE(loadedSnapshot->ephemeris.relativeHumidity, defaults.ephemeris.relativeHumidity);
    QCOMPARE(
        loadedSnapshot->ephemeris.observingWavelengthMicrometers, defaults.ephemeris.observingWavelengthMicrometers
    );
}

void SkySettingsStoreTests::malformedStateValuesFallBackToDefaults()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/live", "maybe");
    settings.setValue("skyContext/timelineToolbarCollapsed", "no");
    settings.setValue("skyContext/speedMultiplier", "fast");
    settings.setValue("skyContext/stepSeconds", "soon");
    settings.setValue("skyContext/magnitudeCutoff", "nan");
    settings.setValue("skyContext/viewCenterAltitudeDeg", "above");
    settings.setValue("skyContext/viewCenterAzimuthDeg", "east-ish");
    settings.setValue("skyContext/viewFieldOfViewDeg", "wide");
    settings.setValue("skyContext/utcEpochMicros", "yesterday");
    settings.setValue("skyContext/latitudeDeg", "north");
    settings.setValue("skyContext/longitudeDeg", "west");
    settings.setValue("skyContext/elevationMeters", "high");
    settings.setValue("skyContext/projectionType", "FishEyePrototype");
    settings.setValue("skyContext/themeId", "unknown-theme");
    settings.setValue("skyContext/overlayLayers/horizon", "sometimes");
    settings.setValue("skyContext/overlayLayers/ecliptic", "on");
    settings.setValue("skyContext/catalogPresetIndex", "primary");
    settings.setValue("skyContext/deepSkyCatalogPresetIndex", "deep");
    settings.setValue("skyContext/logging/logToTerminal", "sure");
    settings.setValue("skyContext/logging/logToFile", "nah");
    settings.setValue("skyContext/logging/logFilePath", "");

    const SkySettingsStore store;
    ignoreSkySettingsFallbackWarnings(17);
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->live, true);
    QCOMPARE(loadedSnapshot->timelineToolbarCollapsed, false);
    QCOMPARE(loadedSnapshot->speedMultiplier, 1.0);
    QCOMPARE(loadedSnapshot->stepSeconds, 60);
    QCOMPARE(loadedSnapshot->magnitudeCutoff, 6.0);
    QCOMPARE(loadedSnapshot->viewCenterAltitudeDeg, 0.0);
    QCOMPARE(loadedSnapshot->viewCenterAzimuthDeg, 0.0);
    QCOMPARE(loadedSnapshot->viewFieldOfViewDeg, 100.0);
    QCOMPARE(loadedSnapshot->utcEpochMicros, 0LL);
    QCOMPARE(loadedSnapshot->latitudeDeg, 0.0);
    QCOMPARE(loadedSnapshot->longitudeDeg, 0.0);
    QCOMPARE(loadedSnapshot->elevationMeters, 0.0);
    QCOMPARE(loadedSnapshot->displayTimeZoneId, QString());
    QCOMPARE(loadedSnapshot->projectionTypeText, QString("FishEyePrototype"));
    QCOMPARE(loadedSnapshot->themeId, QString("unknown-theme"));
    QCOMPARE(loadedSnapshot->overlayLayers.horizon, true);
    QCOMPARE(loadedSnapshot->overlayLayers.ecliptic, true);
    QCOMPARE(loadedSnapshot->catalogPresetIndex, 0);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPresetIndex, 0);
    QCOMPARE(loadedSnapshot->logToTerminal, true);
    QCOMPARE(loadedSnapshot->logToFile, false);
    QCOMPARE(loadedSnapshot->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
    QCOMPARE(loadedSnapshot->ephemerisSettingsPresent, false);
}

void SkySettingsStoreTests::partialStateAndUnknownOverlayKeysAreTolerated()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/searchToolbarCollapsed", true);
    settings.setValue("skyContext/overlayLayers/altAzGrid", false);
    settings.setValue("skyContext/overlayLayers/unusedPrototypeLayer", true);

    const SkySettingsStore store;
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->live, true);
    QCOMPARE(loadedSnapshot->searchToolbarCollapsed, true);
    QCOMPARE(loadedSnapshot->timelineToolbarCollapsed, false);
    QCOMPARE(loadedSnapshot->displayTimeZoneId, QString());
    QCOMPARE(loadedSnapshot->overlayLayers.horizon, true);
    QCOMPARE(loadedSnapshot->overlayLayers.altAzGrid, false);
    QCOMPARE(loadedSnapshot->overlayLayers.deepSkyLabels, true);
    QCOMPARE(loadedSnapshot->logToTerminal, true);
    QCOMPARE(loadedSnapshot->logToFile, false);
    QCOMPARE(loadedSnapshot->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

void SkySettingsStoreTests::malformedEphemerisUserSettingsFallBackToDefaults()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/ephemeris/engineKind", "experimental");
    settings.setValue("skyContext/ephemeris/correctionFlags", "full");
    settings.setValue("skyContext/ephemeris/correctionPresetId", "  ");
    settings.setValue("skyContext/ephemeris/fallbackToSimpleEngine", "occasionally");
    settings.setValue("skyContext/ephemeris/refractionEnabled", "sometimes");
    settings.setValue("skyContext/ephemeris/atmosphericPressureHpa", "heavy");
    settings.setValue("skyContext/ephemeris/atmosphericTemperatureC", "warm");
    settings.setValue("skyContext/ephemeris/relativeHumidity", "humid");
    settings.setValue("skyContext/ephemeris/observingWavelengthMicrometers", "green");
    settings.setValue("skyContext/ephemeris/preferredDataProfileId", "");
    settings.setValue("skyContext/ephemeris/onlineUpdatesEnabled", "maybe");
    settings.setValue("skyContext/ephemeris/updatePresetId", " ");
    settings.setValue("skyContext/ephemeris/updateManifestUrl", "  https://example.invalid/manifest.json  ");

    const SkySettingsStore store;
    ignoreSkySettingsFallbackWarnings(11);
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    const SkyEphemerisUserSettingsSnapshot defaults;
    QCOMPARE(
        static_cast<std::uint8_t>(loadedSnapshot->ephemeris.engineKind), static_cast<std::uint8_t>(defaults.engineKind)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(loadedSnapshot->ephemeris.correctionFlags),
        static_cast<std::uint32_t>(defaults.correctionFlags)
    );
    QCOMPARE(loadedSnapshot->ephemeris.correctionPresetId, defaults.correctionPresetId);
    QCOMPARE(loadedSnapshot->ephemeris.fallbackToSimpleEngine, defaults.fallbackToSimpleEngine);
    QCOMPARE(loadedSnapshot->ephemeris.refractionEnabled, defaults.refractionEnabled);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericPressureHpa, defaults.atmosphericPressureHpa);
    QCOMPARE(loadedSnapshot->ephemeris.atmosphericTemperatureC, defaults.atmosphericTemperatureC);
    QCOMPARE(loadedSnapshot->ephemeris.relativeHumidity, defaults.relativeHumidity);
    QCOMPARE(loadedSnapshot->ephemeris.observingWavelengthMicrometers, defaults.observingWavelengthMicrometers);
    QCOMPARE(loadedSnapshot->ephemeris.preferredDataProfileId, defaults.preferredDataProfileId);
    QCOMPARE(loadedSnapshot->ephemeris.onlineUpdatesEnabled, defaults.onlineUpdatesEnabled);
    QCOMPARE(loadedSnapshot->ephemeris.updatePresetId, defaults.updatePresetId);
    QCOMPARE(loadedSnapshot->ephemeris.updateManifestUrl, QString("https://example.invalid/manifest.json"));
    QCOMPARE(loadedSnapshot->ephemerisSettingsPresent, true);
}

void SkySettingsStoreTests::savesLoadsAndDefaultsLoggingPreferences()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkyPersistedStateSnapshot snapshot;
    snapshot.logToTerminal = false;
    snapshot.logToFile = true;
    snapshot.logFilePath = m_settings.filePath(QStringLiteral("custom-skygate.log"));

    QVERIFY(store.saveState(snapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->logToTerminal, false);
    QCOMPARE(loadedSnapshot->logToFile, true);
    QCOMPARE(loadedSnapshot->logFilePath, snapshot.logFilePath);

    settings.setValue("skyContext/logging/logFilePath", "");
    ignoreSkySettingsFallbackWarnings(1);
    const auto loadedWithBlankPath = store.loadState();
    QVERIFY(loadedWithBlankPath.has_value());
    QCOMPARE(loadedWithBlankPath->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

void SkySettingsStoreTests::malformedLoggingPreferencesFallBackToDefaults()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/version", 3);
    settings.setValue("skyContext/logging/logToTerminal", "maybe");
    settings.setValue("skyContext/logging/logToFile", "eventually");
    settings.setValue("skyContext/logging/logFilePath", "   ");

    const SkySettingsStore store;
    ignoreSkySettingsFallbackWarnings(3);
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->logToTerminal, true);
    QCOMPARE(loadedSnapshot->logToFile, false);
    QCOMPARE(loadedSnapshot->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

QTEST_GUILESS_MAIN(SkySettingsStoreTests)

#include "SkySettingsStoreTests.moc"
