#include <QtTest>

#include "SkySettingsStore.hpp"
#include "SkyLogging.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

void ignoreSkySettingsFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(
                "Invalid .*setting skyContext/.* - using fallback .*"
            )
        );
    }
}

}  // namespace

class SkySettingsStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesAndLoadsStateSnapshot();
    void savesAndLoadsNegativeUtcEpochSeconds();
    void malformedStateValuesFallBackToDefaults();
    void partialStateAndUnknownOverlayKeysAreTolerated();
    void savesLoadsAndClearsCatalogCachesIndependently();
    void partialCatalogCacheSavePreservesConfiguredPeerPath();
    void missingCacheFilesAndMalformedCacheMetadataAreTolerated();
    void savesLoadsAndDefaultsLoggingPreferences();
    void malformedLoggingPreferencesFallBackToDefaults();

private:
    QTemporaryDir m_settingsDir;
};

void SkySettingsStoreTests::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QCoreApplication::setOrganizationName("SkygateTests");
    QCoreApplication::setApplicationName("SkygateUiSettingsStoreTests");
}

void SkySettingsStoreTests::savesAndLoadsStateSnapshot()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot savedSnapshot;
    savedSnapshot.live = false;
    savedSnapshot.timelineToolbarCollapsed = true;
    savedSnapshot.searchToolbarCollapsed = true;
    savedSnapshot.speedMultiplier = 4.0;
    savedSnapshot.stepSeconds = 300;
    savedSnapshot.magnitudeCutoff = 7.5;
    savedSnapshot.viewCenterAltitudeDeg = 18.0;
    savedSnapshot.viewCenterAzimuthDeg = 220.0;
    savedSnapshot.viewFieldOfViewDeg = 74.0;
    savedSnapshot.utcEpochSeconds = 1'717'276'800;
    savedSnapshot.latitudeDeg = 47.0;
    savedSnapshot.longitudeDeg = 8.0;
    savedSnapshot.elevationMeters = 409.0;
    savedSnapshot.locationSourceText = "City";
    savedSnapshot.selectedCityId = "ch-zurich";
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
    savedSnapshot.logFilePath = m_settingsDir.filePath("skygate-test.log");

    QVERIFY(store.saveState(savedSnapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->live, savedSnapshot.live);
    QCOMPARE(loadedSnapshot->timelineToolbarCollapsed, savedSnapshot.timelineToolbarCollapsed);
    QCOMPARE(loadedSnapshot->searchToolbarCollapsed, savedSnapshot.searchToolbarCollapsed);
    QCOMPARE(loadedSnapshot->speedMultiplier, savedSnapshot.speedMultiplier);
    QCOMPARE(loadedSnapshot->stepSeconds, savedSnapshot.stepSeconds);
    QCOMPARE(loadedSnapshot->utcEpochSeconds, savedSnapshot.utcEpochSeconds);
    QCOMPARE(loadedSnapshot->locationSourceText, savedSnapshot.locationSourceText);
    QCOMPARE(loadedSnapshot->selectedCityId, savedSnapshot.selectedCityId);
    QCOMPARE(loadedSnapshot->projectionTypeText, savedSnapshot.projectionTypeText);
    QCOMPARE(loadedSnapshot->themeId, savedSnapshot.themeId);
    QVERIFY(loadedSnapshot->overlayLayers.equals(savedSnapshot.overlayLayers));
    QCOMPARE(loadedSnapshot->catalogPresetIndex, savedSnapshot.catalogPresetIndex);
    QCOMPARE(loadedSnapshot->catalogUrlText, savedSnapshot.catalogUrlText);
    QCOMPARE(
        loadedSnapshot->deepSkyCatalogPresetIndex,
        savedSnapshot.deepSkyCatalogPresetIndex
    );
    QCOMPARE(loadedSnapshot->deepSkyCatalogUrlText, savedSnapshot.deepSkyCatalogUrlText);
    QCOMPARE(loadedSnapshot->logToTerminal, savedSnapshot.logToTerminal);
    QCOMPARE(loadedSnapshot->logToFile, savedSnapshot.logToFile);
    QCOMPARE(loadedSnapshot->logFilePath, savedSnapshot.logFilePath);
}

void SkySettingsStoreTests::savesAndLoadsNegativeUtcEpochSeconds()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot savedSnapshot;
    savedSnapshot.utcEpochSeconds = -63'555'595'200LL;

    QVERIFY(store.saveState(savedSnapshot));
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->utcEpochSeconds, savedSnapshot.utcEpochSeconds);
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
    settings.setValue("skyContext/utcEpochSeconds", "yesterday");
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
    QCOMPARE(loadedSnapshot->utcEpochSeconds, 0LL);
    QCOMPARE(loadedSnapshot->latitudeDeg, 0.0);
    QCOMPARE(loadedSnapshot->longitudeDeg, 0.0);
    QCOMPARE(loadedSnapshot->elevationMeters, 0.0);
    QCOMPARE(loadedSnapshot->projectionTypeText, QString("FishEyePrototype"));
    QCOMPARE(loadedSnapshot->themeId, QString("unknown-theme"));
    QCOMPARE(loadedSnapshot->overlayLayers.horizon, true);
    QCOMPARE(loadedSnapshot->overlayLayers.ecliptic, true);
    QCOMPARE(loadedSnapshot->catalogPresetIndex, 0);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPresetIndex, 0);
    QCOMPARE(loadedSnapshot->logToTerminal, true);
    QCOMPARE(loadedSnapshot->logToFile, false);
    QCOMPARE(loadedSnapshot->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
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
    QCOMPARE(loadedSnapshot->overlayLayers.horizon, true);
    QCOMPARE(loadedSnapshot->overlayLayers.altAzGrid, false);
    QCOMPARE(loadedSnapshot->overlayLayers.deepSkyLabels, true);
    QCOMPARE(loadedSnapshot->logToTerminal, true);
    QCOMPARE(loadedSnapshot->logToFile, false);
    QCOMPARE(loadedSnapshot->logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

void SkySettingsStoreTests::savesLoadsAndClearsCatalogCachesIndependently()
{
    QSettings settings;
    settings.clear();
    settings.setValue(
        "skyContext/catalogCachePath",
        m_settingsDir.filePath("catalog-cache-test.txt")
    );
    settings.setValue(
        "skyContext/deepSkyCatalogCachePath",
        m_settingsDir.filePath("deep-sky-catalog-cache-test.txt")
    );

    SkySettingsStore store;
    SkySettingsStore::CatalogCacheSnapshot savedSnapshot;
    savedSnapshot.sourceLabel = "Saved";
    savedSnapshot.deepSkySourceLabel = "Saved OpenNGC";
    savedSnapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";
    savedSnapshot.deepSkyCatalogPayload =
        "Name;Type;RA;Dec;M;NGC;IC\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;\n";
    savedSnapshot.constellationLineRows = "a|b\n";
    savedSnapshot.constellationLabelRows = "Orion|hip1,hip2\n";
    savedSnapshot.constellationLineSchemaVersion = 4;
    savedSnapshot.constellationCount = 42;

    QVERIFY(store.saveCatalogCache(savedSnapshot));
    const auto loadedSnapshot = store.loadCatalogCache();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->sourceLabel, savedSnapshot.sourceLabel);
    QCOMPARE(loadedSnapshot->deepSkySourceLabel, savedSnapshot.deepSkySourceLabel);
    QCOMPARE(loadedSnapshot->catalogPayload, savedSnapshot.catalogPayload);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPayload, savedSnapshot.deepSkyCatalogPayload);
    QCOMPARE(loadedSnapshot->constellationLineRows, savedSnapshot.constellationLineRows);
    QCOMPARE(
        loadedSnapshot->constellationLineSchemaVersion,
        savedSnapshot.constellationLineSchemaVersion
    );
    QCOMPARE(loadedSnapshot->constellationCount, savedSnapshot.constellationCount);

    QVERIFY(store.clearCatalogCache());
    const auto starClearedSnapshot = store.loadCatalogCache();
    QVERIFY(starClearedSnapshot.has_value());
    QVERIFY(starClearedSnapshot->sourceLabel.isEmpty());
    QVERIFY(starClearedSnapshot->catalogPayload.isEmpty());
    QVERIFY(starClearedSnapshot->constellationLineRows.isEmpty());
    QVERIFY(starClearedSnapshot->constellationLabelRows.isEmpty());
    QCOMPARE(starClearedSnapshot->constellationLineSchemaVersion, 0);
    QCOMPARE(starClearedSnapshot->constellationCount, 0U);
    QCOMPARE(
        starClearedSnapshot->deepSkySourceLabel,
        savedSnapshot.deepSkySourceLabel
    );
    QCOMPARE(
        starClearedSnapshot->deepSkyCatalogPayload,
        savedSnapshot.deepSkyCatalogPayload
    );

    QVERIFY(store.clearDeepSkyCatalogCache());
    QVERIFY(!store.loadCatalogCache().has_value());
}

void SkySettingsStoreTests::partialCatalogCacheSavePreservesConfiguredPeerPath()
{
    QSettings settings;
    settings.clear();
    const QString starCachePath = m_settingsDir.filePath("partial-star-cache.txt");
    const QString deepSkyCachePath = m_settingsDir.filePath("partial-deep-sky-cache.txt");
    settings.setValue("skyContext/catalogCachePath", starCachePath);
    settings.setValue("skyContext/deepSkyCatalogCachePath", deepSkyCachePath);

    SkySettingsStore store;
    SkySettingsStore::CatalogCacheSnapshot starSnapshot;
    starSnapshot.sourceLabel = "Saved";
    starSnapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";
    QVERIFY(store.saveCatalogCache(starSnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);

    SkySettingsStore::CatalogCacheSnapshot deepSkySnapshot;
    deepSkySnapshot.deepSkySourceLabel = "Saved OpenNGC";
    deepSkySnapshot.deepSkyCatalogPayload =
        "Name;Type;RA;Dec;M;NGC;IC\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;\n";
    QVERIFY(store.saveCatalogCache(deepSkySnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);
    QVERIFY(QFileInfo::exists(deepSkyCachePath));
}

void SkySettingsStoreTests::missingCacheFilesAndMalformedCacheMetadataAreTolerated()
{
    QSettings settings;
    settings.clear();
    settings.setValue(
        "skyContext/catalogCachePath",
        m_settingsDir.filePath("missing-star-cache.csv")
    );
    settings.setValue("skyContext/catalogSourceLabel", "Missing HYG");
    settings.setValue("skyContext/catalogConstellationLineSchemaVersion", "latest");
    settings.setValue("skyContext/catalogConstellationCount", "many");

    const SkySettingsStore store;
    QVERIFY(!store.loadCatalogCache().has_value());

    const QString cachePath = m_settingsDir.filePath("malformed-metadata-cache.csv");
    QFile cacheFile(cachePath);
    QVERIFY(cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(cacheFile.write(
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n"
    ) > 0);
    cacheFile.close();

    settings.setValue("skyContext/catalogCachePath", cachePath);
    settings.setValue("skyContext/catalogConstellationLineSchemaVersion", "latest");
    settings.setValue("skyContext/catalogConstellationCount", "many");
    ignoreSkySettingsFallbackWarnings(2);
    const auto loadedSnapshot = store.loadCatalogCache();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->sourceLabel, QString("Missing HYG"));
    QCOMPARE(loadedSnapshot->constellationLineSchemaVersion, 0);
    QCOMPARE(loadedSnapshot->constellationCount, 0U);
    QVERIFY(loadedSnapshot->deepSkyCatalogPayload.isEmpty());
}

void SkySettingsStoreTests::savesLoadsAndDefaultsLoggingPreferences()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.logToTerminal = false;
    snapshot.logToFile = true;
    snapshot.logFilePath = m_settingsDir.filePath("custom-skygate.log");

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
    QCOMPARE(
        loadedWithBlankPath->logFilePath,
        skygate::ui::SkyLogging::defaultLogFilePath()
    );
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
