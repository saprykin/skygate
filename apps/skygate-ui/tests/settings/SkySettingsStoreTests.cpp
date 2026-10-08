#include "CatalogCacheTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "LogCapture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyLogging.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QtTest>

#include <cstdint>
#include <utility>

namespace {

void ignoreSkySettingsFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid .*setting skyContext/.* - using fallback .*"));
    }
}

SkySettingsStore::CatalogCollectionCacheSnapshot sampleCollectionSnapshot()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore::CatalogSourceCacheRecord star;
    star.instanceId = QStringLiteral("preset:hyg_v42");
    star.descriptorId = QStringLiteral("hyg_v42");
    star.title = QStringLiteral("HYG v4.2");
    star.version = QStringLiteral("v4.2");
    star.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/lines.json")};
    star.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.order = 0;
    star.payload = skygate::ui::tests::sampleHygCsvPayload();
    star.binaryPayload = QByteArray("binary-star");
    star.constellationLineRows = "hip_1|hip_2\n";
    star.constellationAnchorGroupRows = "Demo|hip_1,hip_2\n";
    star.constellationLineSchemaVersion = 4;
    star.constellationCount = 1;
    snapshot.sources.push_back(std::move(star));

    SkySettingsStore::CatalogSourceCacheRecord deepSky;
    deepSky.instanceId = QStringLiteral("preset:open_ngc");
    deepSky.descriptorId = QStringLiteral("open_ngc");
    deepSky.title = QStringLiteral("OpenNGC");
    deepSky.version = QStringLiteral("v20260307");
    deepSky.urls = QStringList{QStringLiteral("https://example.test/NGC.csv")};
    deepSky.policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly;
    deepSky.enabled = true;
    deepSky.order = 1;
    deepSky.payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    deepSky.binaryPayload = QByteArray("binary-dso");
    snapshot.sources.push_back(std::move(deepSky));
    return snapshot;
}

SkySettingsStore::CatalogSourceCacheRecord makeCollectionRecord(
    const QString& instanceId, const QString& title, const QByteArray& rawPayload, const QByteArray& binaryPayload
)
{
    SkySettingsStore::CatalogSourceCacheRecord record;
    record.instanceId = instanceId;
    record.title = title;
    record.urls = QStringList{QStringLiteral("https://example.test/%1.csv").arg(instanceId)};
    record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    record.enabled = true;
    record.payload = rawPayload;
    record.binaryPayload = binaryPayload;
    return record;
}

SkySettingsStore::CatalogCollectionCacheSnapshot
makeCollectionSnapshot(QVector<SkySettingsStore::CatalogSourceCacheRecord> records)
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    for (std::size_t index = 0; index < records.size(); ++index) {
        records[index].order = static_cast<int>(index);
    }
    snapshot.sources = std::move(records);
    return snapshot;
}

// Two committed sources whose titles, raw bytes, and binary bytes all differ
// from the update a failure test attempts, so a mixed snapshot is visible.
SkySettingsStore::CatalogCollectionCacheSnapshot oldCollectionSnapshot()
{
    return makeCollectionSnapshot(
        {makeCollectionRecord(
             QStringLiteral("custom:alpha"),
             QStringLiteral("Old alpha"),
             QByteArray("old raw alpha"),
             QByteArray("old binary alpha")
         ),
         makeCollectionRecord(
             QStringLiteral("custom:beta"),
             QStringLiteral("Old beta"),
             QByteArray("old raw beta"),
             QByteArray("old binary beta")
         )}
    );
}

SkySettingsStore::CatalogCollectionCacheSnapshot newCollectionSnapshot()
{
    return makeCollectionSnapshot(
        {makeCollectionRecord(
             QStringLiteral("custom:alpha"),
             QStringLiteral("New alpha"),
             QByteArray("new raw alpha"),
             QByteArray("new binary alpha")
         ),
         makeCollectionRecord(
             QStringLiteral("custom:beta"),
             QStringLiteral("New beta"),
             QByteArray("new raw beta"),
             QByteArray("new binary beta")
         )}
    );
}

void compareCollectionRecords(
    const SkySettingsStore::CatalogCollectionCacheSnapshot& loaded,
    const SkySettingsStore::CatalogCollectionCacheSnapshot& expected
)
{
    QCOMPARE(loaded.sources.size(), expected.sources.size());
    for (std::size_t index = 0; index < expected.sources.size(); ++index) {
        QCOMPARE(loaded.sources[index].instanceId, expected.sources[index].instanceId);
        QCOMPARE(loaded.sources[index].title, expected.sources[index].title);
        QCOMPARE(loaded.sources[index].payload, expected.sources[index].payload);
        QCOMPARE(loaded.sources[index].binaryPayload, expected.sources[index].binaryPayload);
    }
}

QString prepareCollectionCacheDirectory(const skygate::ui::tests::SettingsTestFixture& settings)
{
    settings.resetSettingsWithCatalogCachePaths();
    const QString directory = settings.filePath(QStringLiteral("collection-cache"));
    QSettings storeSettings;
    storeSettings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), directory);
    QDir(directory).removeRecursively();
    return directory;
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
    void malformedStateValuesFallBackToDefaults();
    void partialStateAndUnknownOverlayKeysAreTolerated();
    void savesLoadsAndClearsCatalogCachesIndependently();
    void savesLoadsAndClearsCatalogCollectionCache();
    void freshStoreReportsNoCommittedCollection();
    void emptyCatalogCollectionCacheIsExplicitlyPersisted();
    void failedCollectionSaveKeepsLegacyCacheUnmigrated();
    void clearCatalogSourceCacheEvictsPayloadAndKeepsConfiguration();
    void unreadableSourcePayloadIsReportedDistinctlyFromEviction();
    void legacyFlatCollectionRecordsStillLoad();
    void interruptedGenerationStagingKeepsFlatCollectionLoaded();
    void emptyLegacyCollectionStillLoadsWithoutGenerationArtifacts();
    void missingManifestWithGenerationRecordsDoesNotLoadEmptyCollection();
    void missingManifestWithGenerationSidecarsLogsManifestLoss();
    void lostManifestAfterCommittedCollectionIsNotFirstUse();
    void stageOnlyGenerationIsNotProofOfCommit();
    void truncatedManifestIsUnusableButRecoverable();
    void missingCommittedGenerationRecordsAreUnusable();
    void failedLaterSourceRawWriteKeepsCommittedSnapshot();
    void failedLaterSourceBinaryWriteKeepsCommittedSnapshot();
    void failedMetadataPublicationKeepsCommittedSnapshot();
    void failedManifestPublicationKeepsCommittedGenerationIntact();
    void failedFirstCollectionPublicationKeepsLegacyCacheReadable();
    void successfulCollectionUpdateReplacesCommittedGeneration();
    void failedCollectionSaveLogsExplicitError();
    void partialCatalogCacheSavePreservesConfiguredPeerPath();
    void missingCacheFilesAndMalformedCacheMetadataAreTolerated();
    void savesLoadsAndClearsEphemerisDataCacheMetadata();
    void ephemerisDataCachePathsRoundTripExactly();
    void partialAndMalformedEphemerisDataCacheMetadataFallsBack();
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
    SkySettingsStore::StateSnapshot savedSnapshot;
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

void SkySettingsStoreTests::savesLoadsAndClearsCatalogCachesIndependently()
{
    m_settings.resetSettingsWithCatalogCachePaths(
        QStringLiteral("catalog-cache-test.txt"), QStringLiteral("deep-sky-catalog-cache-test.txt")
    );

    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot savedSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.constellationLineRows = "a|b\n",
         .constellationAnchorGroupRows = "Orion|hip1,hip2\n",
         .constellationCount = 42}
    );

    QVERIFY(store.saveCatalogCache(savedSnapshot));
    const auto loadedSnapshot = store.loadCatalogCache();
    QVERIFY(loadedSnapshot.has_value());
    QCOMPARE(loadedSnapshot->sourceLabel, savedSnapshot.sourceLabel);
    QCOMPARE(loadedSnapshot->deepSkySourceLabel, savedSnapshot.deepSkySourceLabel);
    QCOMPARE(loadedSnapshot->catalogPayload, savedSnapshot.catalogPayload);
    QCOMPARE(loadedSnapshot->deepSkyCatalogPayload, savedSnapshot.deepSkyCatalogPayload);
    QCOMPARE(loadedSnapshot->constellationLineRows, savedSnapshot.constellationLineRows);
    QCOMPARE(loadedSnapshot->constellationLineSchemaVersion, savedSnapshot.constellationLineSchemaVersion);
    QCOMPARE(loadedSnapshot->constellationCount, savedSnapshot.constellationCount);

    QVERIFY(store.clearCatalogCache());
    const auto starClearedSnapshot = store.loadCatalogCache();
    QVERIFY(starClearedSnapshot.has_value());
    QVERIFY(starClearedSnapshot->sourceLabel.isEmpty());
    QVERIFY(starClearedSnapshot->catalogPayload.isEmpty());
    QVERIFY(starClearedSnapshot->constellationLineRows.isEmpty());
    QVERIFY(starClearedSnapshot->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(starClearedSnapshot->constellationLineSchemaVersion, 0);
    QCOMPARE(starClearedSnapshot->constellationCount, 0U);
    QCOMPARE(starClearedSnapshot->deepSkySourceLabel, savedSnapshot.deepSkySourceLabel);
    QCOMPARE(starClearedSnapshot->deepSkyCatalogPayload, savedSnapshot.deepSkyCatalogPayload);

    QVERIFY(store.clearDeepSkyCatalogCache());
    QVERIFY(!store.loadCatalogCache().has_value());
}

void SkySettingsStoreTests::savesLoadsAndClearsCatalogCollectionCache()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();

    SkySettingsStore store;
    const auto savedSnapshot = sampleCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(savedSnapshot));

    const SkySettingsStore::CatalogCollectionCacheLoadResult loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    const auto& loadedSnapshot = loaded.snapshot;
    QCOMPARE(loadedSnapshot.schemaVersion, savedSnapshot.schemaVersion);
    QCOMPARE(loadedSnapshot.binarySchemaVersion, savedSnapshot.binarySchemaVersion);
    QCOMPARE(loadedSnapshot.sources.size(), 2);
    QCOMPARE(loadedSnapshot.sources[0].instanceId, savedSnapshot.sources[0].instanceId);
    QCOMPARE(loadedSnapshot.sources[0].descriptorId, savedSnapshot.sources[0].descriptorId);
    QCOMPARE(loadedSnapshot.sources[0].title, savedSnapshot.sources[0].title);
    QCOMPARE(loadedSnapshot.sources[0].urls, savedSnapshot.sources[0].urls);
    QCOMPARE(loadedSnapshot.sources[0].relatedDatasetUrls, savedSnapshot.sources[0].relatedDatasetUrls);
    QCOMPARE(static_cast<int>(loadedSnapshot.sources[0].policy), static_cast<int>(savedSnapshot.sources[0].policy));
    QCOMPARE(loadedSnapshot.sources[0].enabled, savedSnapshot.sources[0].enabled);
    QCOMPARE(loadedSnapshot.sources[0].payload, savedSnapshot.sources[0].payload);
    QCOMPARE(loadedSnapshot.sources[0].binaryPayload, savedSnapshot.sources[0].binaryPayload);
    QCOMPARE(loadedSnapshot.sources[0].constellationLineRows, savedSnapshot.sources[0].constellationLineRows);
    QCOMPARE(loadedSnapshot.sources[0].constellationCount, savedSnapshot.sources[0].constellationCount);
    QCOMPARE(loadedSnapshot.sources[1].instanceId, savedSnapshot.sources[1].instanceId);
    QCOMPARE(loadedSnapshot.sources[1].payload, savedSnapshot.sources[1].payload);
    QCOMPARE(loadedSnapshot.sources[1].binaryPayload, savedSnapshot.sources[1].binaryPayload);
    QVERIFY(loaded.unreadablePayloadInstanceIds.isEmpty());

    QVERIFY(store.clearCatalogCollectionCache());
    const auto afterClear = store.loadCatalogCollectionCache();
    QCOMPARE(afterClear.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
}

void SkySettingsStoreTests::freshStoreReportsNoCommittedCollection()
{
    prepareCollectionCacheDirectory(m_settings);

    const SkySettingsStore store;
    const auto firstUse = store.loadCatalogCollectionCache();
    QCOMPARE(firstUse.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
    QCOMPARE(firstUse.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::None);
    QVERIFY(firstUse.diagnostic.isEmpty());
    QVERIFY(firstUse.snapshot.sources.isEmpty());
    QVERIFY(firstUse.unreadablePayloadInstanceIds.isEmpty());
}

void SkySettingsStoreTests::emptyCatalogCollectionCacheIsExplicitlyPersisted()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(skygate::ui::tests::sampleCatalogCacheSnapshot()));

    SkySettingsStore::CatalogCollectionCacheSnapshot emptySnapshot;
    emptySnapshot.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    QVERIFY(store.saveCatalogCollectionCache(emptySnapshot));

    // An intentionally empty collection is stored, not erased: the committed
    // manifest distinguishes it from "no collection was ever stored" and keeps
    // the legacy two-slot cache retired.
    const auto loaded = store.loadCatalogCollectionCache();
    QCOMPARE(loaded.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Loaded);
    QVERIFY(loaded.snapshot.sources.isEmpty());
    QCOMPARE(
        loaded.snapshot.schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );

    // A second restart persists the same intentionally empty configuration.
    QVERIFY(store.saveCatalogCollectionCache(emptySnapshot));
    const auto reloaded = store.loadCatalogCollectionCache();
    QCOMPARE(reloaded.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Loaded);
    QVERIFY(reloaded.snapshot.sources.isEmpty());

    // The legacy cache stays readable; the committed collection retires the
    // fallback without deleting recoverable data.
    QVERIFY(store.loadCatalogCache().has_value());

    // Resetting the stored configuration is the operation that removes the
    // committed collection again.
    QVERIFY(store.clearCatalogCollectionCache());
    QCOMPARE(
        store.loadCatalogCollectionCache().state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent
    );
}

void SkySettingsStoreTests::failedCollectionSaveKeepsLegacyCacheUnmigrated()
{
    m_settings.resetSettingsWithCatalogCachePaths();

    SkySettingsStore store;
    const auto legacySnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Legacy custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
    );
    QVERIFY(store.saveCatalogCache(legacySnapshot));

    // Point the collection cache directory under a regular file so the payload
    // sidecar write fails before any collection metadata is committed.
    const QString blockerPath = m_settings.filePath(QStringLiteral("collection-blocker"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    blocker.write("x");
    blocker.close();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), blockerPath + QStringLiteral("/nested/cache")
    );

    QVERIFY(!store.saveCatalogCollectionCache(sampleCollectionSnapshot()));

    // The failed save neither records a completed migration nor destroys the
    // last readable legacy data.
    const auto failedLoad = store.loadCatalogCollectionCache();
    QCOMPARE(failedLoad.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
    QCOMPARE(failedLoad.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::None);
    const auto stillReadable = store.loadCatalogCache();
    QVERIFY(stillReadable.has_value());
    QCOMPARE(stillReadable->sourceLabel, legacySnapshot.sourceLabel);
    QCOMPARE(stillReadable->catalogPayload, legacySnapshot.catalogPayload);
    QCOMPARE(stillReadable->deepSkyCatalogPayload, legacySnapshot.deepSkyCatalogPayload);
}

void SkySettingsStoreTests::clearCatalogSourceCacheEvictsPayloadAndKeepsConfiguration()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto savedSnapshot = sampleCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(savedSnapshot));

    const QDir cacheDir(directory);
    QCOMPARE(cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files).size(), 4);

    QVERIFY(store.clearCatalogSourceCache(QStringLiteral("preset:hyg_v42")));

    // Only the cleared source's payload sidecars are gone; the peer keeps both
    // of its files.
    QCOMPARE(cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files).size(), 2);

    const auto evicted = store.loadCatalogCollectionCache();
    QVERIFY(evicted.isLoaded());
    QCOMPARE(evicted.snapshot.sources.size(), 2);

    // The cleared source keeps every configuration field and drops only its
    // disposable payload references and the related dataset they carry. The
    // eviction is deliberate, so the missing payload is not corrupt data.
    const auto& loadedStar = evicted.snapshot.sources[0];
    QCOMPARE(loadedStar.instanceId, savedSnapshot.sources[0].instanceId);
    QCOMPARE(loadedStar.descriptorId, savedSnapshot.sources[0].descriptorId);
    QCOMPARE(loadedStar.title, savedSnapshot.sources[0].title);
    QCOMPARE(loadedStar.version, savedSnapshot.sources[0].version);
    QCOMPARE(loadedStar.url, savedSnapshot.sources[0].url);
    QCOMPARE(loadedStar.urls, savedSnapshot.sources[0].urls);
    QCOMPARE(loadedStar.relatedDatasetUrls, savedSnapshot.sources[0].relatedDatasetUrls);
    QCOMPARE(loadedStar.archiveSelector, savedSnapshot.sources[0].archiveSelector);
    QCOMPARE(loadedStar.schemaHint, savedSnapshot.sources[0].schemaHint);
    QCOMPARE(loadedStar.attribution, savedSnapshot.sources[0].attribution);
    QCOMPARE(loadedStar.policy, savedSnapshot.sources[0].policy);
    QCOMPARE(loadedStar.enabled, savedSnapshot.sources[0].enabled);
    QCOMPARE(loadedStar.bundled, savedSnapshot.sources[0].bundled);
    QCOMPARE(loadedStar.order, savedSnapshot.sources[0].order);
    QVERIFY(loadedStar.payload.isEmpty());
    QVERIFY(loadedStar.binaryPayload.isEmpty());
    QVERIFY(loadedStar.constellationLineRows.isEmpty());
    QVERIFY(loadedStar.constellationAnchorGroupRows.isEmpty());
    QCOMPARE(loadedStar.constellationCount, std::size_t{0});

    // The peer record and its payload are untouched.
    QCOMPARE(evicted.snapshot.sources[1].instanceId, savedSnapshot.sources[1].instanceId);
    QCOMPARE(evicted.snapshot.sources[1].payload, savedSnapshot.sources[1].payload);
    QCOMPARE(evicted.snapshot.sources[1].binaryPayload, savedSnapshot.sources[1].binaryPayload);
    QVERIFY(evicted.unreadablePayloadInstanceIds.isEmpty());
}

void SkySettingsStoreTests::unreadableSourcePayloadIsReportedDistinctlyFromEviction()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto savedSnapshot = sampleCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(savedSnapshot));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // Remove one source's raw sidecar while its record keeps the reference. The
    // other record stays intact, so only the corrupt one is reported.
    const QString corruptedRawPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration, QStringLiteral("preset:hyg_v42"), QStringLiteral(".txt")
    );
    QVERIFY(QFile::remove(corruptedRawPath));

    const auto corrupted = store.loadCatalogCollectionCache();
    QVERIFY(corrupted.isLoaded());
    QCOMPARE(corrupted.snapshot.sources.size(), 2);
    QVERIFY(corrupted.snapshot.sources[0].payload.isEmpty());
    QCOMPARE(corrupted.snapshot.sources[0].binaryPayload, savedSnapshot.sources[0].binaryPayload);
    QCOMPARE(corrupted.snapshot.sources[1].payload, savedSnapshot.sources[1].payload);
    QCOMPARE(corrupted.unreadablePayloadInstanceIds, QStringList{QStringLiteral("preset:hyg_v42")});

    // A deliberate eviction drops the payload references, so the record stays
    // readable without being reported as corrupt data.
    QVERIFY(store.clearCatalogSourceCache(QStringLiteral("preset:open_ngc")));
    const auto evicted = store.loadCatalogCollectionCache();
    QVERIFY(evicted.isLoaded());
    QVERIFY(evicted.snapshot.sources[1].payload.isEmpty());
    QVERIFY(evicted.snapshot.sources[1].binaryPayload.isEmpty());
    QCOMPARE(evicted.unreadablePayloadInstanceIds, QStringList{QStringLiteral("preset:hyg_v42")});
}

void SkySettingsStoreTests::legacyFlatCollectionRecordsStillLoad()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);
    QVERIFY(QDir().mkpath(directory));

    const QString payloadPath = QDir(directory).filePath(QStringLiteral("legacy-payload.txt"));
    QFile payloadFile(payloadPath);
    QVERIFY(payloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(payloadFile.write("legacy raw payload"), qint64(18));
    payloadFile.close();

    // A collection written by an earlier release keeps its version marker and
    // record groups directly in the settings file, without a generation
    // manifest. Such settings must still load after this change.
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
    settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
    settings.beginGroup(QStringLiteral("catalogSources/catalog-source-legacy"));
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:legacy"));
    settings.setValue(QStringLiteral("title"), QStringLiteral("Legacy title"));
    settings.setValue(QStringLiteral("payloadPath"), payloadPath);
    settings.endGroup();
    settings.sync();

    skygate::ui::tests::LogCapture capture;
    SkySettingsStore store;
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QCOMPARE(loaded.snapshot.schemaVersion, 2);
    QCOMPARE(loaded.snapshot.binarySchemaVersion, 7);
    QCOMPARE(loaded.snapshot.sources.size(), 1);
    QCOMPARE(loaded.snapshot.sources[0].instanceId, QString("custom:legacy"));
    QCOMPARE(loaded.snapshot.sources[0].title, QString("Legacy title"));
    QCOMPARE(loaded.snapshot.sources[0].payload, QByteArray("legacy raw payload"));

    // This settings file has no generation-format artifacts, so the absent
    // manifest is the normal pre-generation state and must stay quiet.
    const QString messages = capture.joinedMessages();
    QVERIFY(!messages.contains(QStringLiteral("Catalog collection manifest is missing")));
    QVERIFY(!messages.contains(QStringLiteral("stale catalog collection version marker")));
}

void SkySettingsStoreTests::interruptedGenerationStagingKeepsFlatCollectionLoaded()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);
    QVERIFY(QDir().mkpath(directory));

    const QString firstPayloadPath = QDir(directory).filePath(QStringLiteral("flat-first.txt"));
    const QString secondPayloadPath = QDir(directory).filePath(QStringLiteral("flat-second.txt"));
    const auto writePayload = [](const QString& path, const QByteArray& payload) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(payload) == payload.size();
    };
    QVERIFY(writePayload(firstPayloadPath, QByteArray("flat raw first")));
    QVERIFY(writePayload(secondPayloadPath, QByteArray("flat raw second")));

    // A collection written by an earlier release keeps its version marker and
    // sidecar-named record groups directly in the settings file.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
        // The group names sort opposite to the order values on purpose: only
        // sorting by the order field yields the configured sequence.
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-second"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:first"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_first"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat First"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/first.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
        );
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 0);
        settings.setValue(QStringLiteral("payloadPath"), firstPayloadPath);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-first"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:second"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_second"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat Second"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/second.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly)
        );
        settings.setValue(QStringLiteral("enabled"), false);
        settings.setValue(QStringLiteral("order"), 1);
        settings.setValue(QStringLiteral("payloadPath"), secondPayloadPath);
        settings.endGroup();
        settings.sync();
    }

    SkySettingsStore store;
    const auto flat = store.loadCatalogCollectionCache();
    QVERIFY(flat.isLoaded());
    QCOMPARE(flat.snapshot.schemaVersion, 2);
    QCOMPARE(flat.snapshot.binarySchemaVersion, 7);
    QCOMPARE(flat.snapshot.sources.size(), 2);
    QCOMPARE(flat.snapshot.sources[0].instanceId, QString("flat:first"));
    QCOMPARE(flat.snapshot.sources[0].order, 0);
    QCOMPARE(flat.snapshot.sources[0].payload, QByteArray("flat raw first"));
    QCOMPARE(flat.snapshot.sources[1].instanceId, QString("flat:second"));
    QCOMPARE(flat.snapshot.sources[1].order, 1);
    QCOMPARE(flat.snapshot.sources[1].payload, QByteArray("flat raw second"));

    // An interrupted first upgrade staged generation records and a sidecar
    // without publishing a manifest. The older flat collection is already
    // committed configuration, so it stays the loaded collection while the
    // staged generation is neither promoted nor allowed to outrank it.
    const QString stagedSidecarPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, 4, QStringLiteral("staged:source"), QStringLiteral(".txt")
    );
    QVERIFY(writePayload(stagedSidecarPath, QByteArray("staged raw payload")));
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
        settings.beginGroup(QStringLiteral("catalogSources/4/catalog-source-staged"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("staged:source"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Source"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.setValue(QStringLiteral("payloadPath"), stagedSidecarPath);
        settings.endGroup();
        settings.sync();
    }

    const auto mixed = store.loadCatalogCollectionCache();
    QVERIFY(mixed.isLoaded());
    QCOMPARE(mixed.snapshot.schemaVersion, 2);
    QCOMPARE(mixed.snapshot.binarySchemaVersion, 7);
    QCOMPARE(mixed.snapshot.sources.size(), 2);
    QCOMPARE(mixed.snapshot.sources[0].instanceId, QString("flat:first"));
    QCOMPARE(mixed.snapshot.sources[0].descriptorId, QString("flat_first"));
    QCOMPARE(mixed.snapshot.sources[0].title, QString("Flat First"));
    QCOMPARE(mixed.snapshot.sources[0].urls, QStringList{QStringLiteral("https://example.test/first.csv")});
    QCOMPARE(mixed.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(mixed.snapshot.sources[0].enabled);
    QCOMPARE(mixed.snapshot.sources[0].order, 0);
    QCOMPARE(mixed.snapshot.sources[0].payload, QByteArray("flat raw first"));
    QCOMPARE(mixed.snapshot.sources[1].instanceId, QString("flat:second"));
    QCOMPARE(mixed.snapshot.sources[1].descriptorId, QString("flat_second"));
    QCOMPARE(mixed.snapshot.sources[1].title, QString("Flat Second"));
    QCOMPARE(mixed.snapshot.sources[1].urls, QStringList{QStringLiteral("https://example.test/second.csv")});
    QCOMPARE(mixed.snapshot.sources[1].policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(!mixed.snapshot.sources[1].enabled);
    QCOMPARE(mixed.snapshot.sources[1].order, 1);
    QCOMPARE(mixed.snapshot.sources[1].payload, QByteArray("flat raw second"));
    QVERIFY(!mixed.diagnostic.isEmpty());

    // Reading the older collection neither consumes nor rewrites the staged
    // generation artifacts.
    QFile stagedSidecar(stagedSidecarPath);
    QVERIFY(stagedSidecar.open(QIODevice::ReadOnly));
    QCOMPARE(stagedSidecar.readAll(), QByteArray("staged raw payload"));
}

void SkySettingsStoreTests::emptyLegacyCollectionStillLoadsWithoutGenerationArtifacts()
{
    prepareCollectionCacheDirectory(m_settings);

    // A pre-migration settings file can also record a committed-but-empty
    // collection: the marker is present while no flat record groups and no
    // generation artifacts exist. That empty snapshot stays the answer and the
    // absent manifest is not a loss to report.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 3);
        settings.sync();
    }

    skygate::ui::tests::LogCapture capture;
    const SkySettingsStore store;
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QVERIFY(loaded.snapshot.sources.isEmpty());
    QCOMPARE(loaded.snapshot.schemaVersion, 3);
    QVERIFY(!capture.joinedMessages().contains(QStringLiteral("Catalog collection manifest is missing")));
}

void SkySettingsStoreTests::missingManifestWithGenerationRecordsDoesNotLoadEmptyCollection()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    // A settings file written by an earlier release carried the legacy
    // collection marker and its flat records. The generation-format save
    // migrates the records but leaves the marker behind, so the marker alone
    // no longer describes what is stored.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-legacy"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:legacy"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Legacy title"));
        settings.endGroup();
        settings.sync();
    }

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const QString rawSidecarPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QFile rawSidecar(rawSidecarPath);
    QVERIFY(rawSidecar.open(QIODevice::ReadOnly));
    const QByteArray committedRawPayload = rawSidecar.readAll();
    rawSidecar.close();

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QByteArray committedManifest = manifestFile.readAll();
    manifestFile.close();

    const QDir cacheDir(directory);
    const QStringList sidecarsBefore = cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files);
    QCOMPARE(sidecarsBefore.size(), 4);
    QVERIFY(QFile::remove(manifestPath));

    // Without the manifest the stale marker points at flat records the
    // migration already replaced. Settings written before commit knowledge
    // existed carry no record of the committed generation, so only the stale
    // marker remains; reporting an empty collection instead of an unusable
    // committed state would let its caller commit that emptiness over the
    // committed generation.
    {
        QSettings settings;
        settings.remove(QStringLiteral("skyContext/catalogCollectionCommittedGeneration"));
        settings.sync();
    }
    skygate::ui::tests::LogCapture capture;
    const auto unreadable = store.loadCatalogCollectionCache();
    QCOMPARE(unreadable.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Unusable);
    QCOMPARE(unreadable.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::LegacyRecordsReplaced);
    QVERIFY(!unreadable.diagnostic.isEmpty());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog collection manifest is missing")));
    QVERIFY(messages.contains(QStringLiteral("stale catalog collection version marker")));

    // Loading nothing is not an implicit cleanup: every sidecar is still
    // present with its bytes and the committed records were not touched.
    QCOMPARE(cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files), sidecarsBefore);
    QFile rawSidecarAfter(rawSidecarPath);
    QVERIFY(rawSidecarAfter.open(QIODevice::ReadOnly));
    QCOMPARE(rawSidecarAfter.readAll(), committedRawPayload);
    rawSidecarAfter.close();
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("catalogSources/%1").arg(committedGeneration));
        QCOMPARE(settings.childGroups().size(), 2);
        settings.endGroup();
    }

    // Restoring the manifest alone brings the committed collection back,
    // proving the unusable result above left nothing else behind.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifest), qint64(committedManifest.size()));
    restoredManifest.close();
    const auto reloaded = store.loadCatalogCollectionCache();
    QVERIFY(reloaded.isLoaded());
    compareCollectionRecords(reloaded.snapshot, committed);
}

void SkySettingsStoreTests::missingManifestWithGenerationSidecarsLogsManifestLoss()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));

    // A settings reset can remove the generation records and the commit record
    // while the sidecar files survive. The leftover files are not proof that a
    // collection was committed, so the outcome stays first use, but the data
    // that was ignored must not be silent.
    {
        QSettings settings;
        settings.remove(QStringLiteral("catalogSources"));
        settings.remove(QStringLiteral("skyContext/catalogCollectionCommittedGeneration"));
        settings.sync();
    }
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(directory)));

    skygate::ui::tests::LogCapture capture;
    const auto stagedLeftovers = store.loadCatalogCollectionCache();
    QCOMPARE(stagedLeftovers.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
    QVERIFY(!stagedLeftovers.diagnostic.isEmpty());
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Catalog collection manifest is missing")));
}

void SkySettingsStoreTests::lostManifestAfterCommittedCollectionIsNotFirstUse()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // The commit record survives on its own and names the published generation.
    {
        QSettings settings;
        QCOMPARE(
            settings.value(QStringLiteral("skyContext/catalogCollectionCommittedGeneration")).toULongLong(),
            committedGeneration
        );
    }

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();
    QVERIFY(QFile::remove(manifestPath));

    const auto lostManifest = store.loadCatalogCollectionCache();
    QCOMPARE(lostManifest.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Unusable);
    QCOMPARE(
        lostManifest.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::CommittedManifestMissing
    );
    QVERIFY(lostManifest.diagnostic.contains(QStringLiteral("recorded as committed")));

    // The recorded generation is a proven recovery point: restoring the
    // manifest brings the committed collection back unchanged.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    compareCollectionRecords(recovered.snapshot, committed);
}

void SkySettingsStoreTests::stageOnlyGenerationIsNotProofOfCommit()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);
    QVERIFY(QDir().mkpath(directory));

    // A first save interrupted before its manifest was published leaves staged
    // records and a sidecar behind. Nothing proves that a collection was ever
    // committed, so the outcome stays first use.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
        settings.beginGroup(QStringLiteral("catalogSources/4/catalog-source-staged"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:staged"));
        settings.endGroup();
        settings.sync();
    }
    QFile stagedSidecar(
        skygate::ui::tests::stagedCatalogSourceSidecarPath(
            directory, 4, QStringLiteral("custom:staged"), QStringLiteral(".txt")
        )
    );
    QVERIFY(stagedSidecar.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(stagedSidecar.write("staged raw payload"), qint64(18));
    stagedSidecar.close();

    SkySettingsStore store;
    const auto firstUse = store.loadCatalogCollectionCache();
    QCOMPARE(firstUse.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
    QCOMPARE(firstUse.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::None);
    QVERIFY(!firstUse.diagnostic.isEmpty());

    // Once a manifest has committed a generation, losing it is unusable
    // committed configuration instead of first use.
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(directory)));
    const auto lostCommitted = store.loadCatalogCollectionCache();
    QCOMPARE(lostCommitted.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Unusable);
    QCOMPARE(
        lostCommitted.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::CommittedManifestMissing
    );
}

void SkySettingsStoreTests::truncatedManifestIsUnusableButRecoverable()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();

    // A truncated manifest is committed configuration that cannot be read, not
    // first use, even while the committed generation itself stays intact.
    const QByteArray truncatedContent = QByteArrayLiteral("schemaVersion=1\nbinarySchemaVersion=4\ngenera");
    QFile truncatedManifest(manifestPath);
    QVERIFY(truncatedManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(truncatedManifest.write(truncatedContent), qint64(truncatedContent.size()));
    truncatedManifest.close();

    skygate::ui::tests::LogCapture capture;
    const auto truncated = store.loadCatalogCollectionCache();
    QCOMPARE(truncated.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Unusable);
    QCOMPARE(truncated.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::ManifestUnreadable);
    QVERIFY(!truncated.diagnostic.isEmpty());
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Unreadable catalog collection manifest")));

    // Restoring the manifest recovers the committed collection unchanged.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    compareCollectionRecords(recovered.snapshot, committed);
}

void SkySettingsStoreTests::missingCommittedGenerationRecordsAreUnusable()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // The manifest survives while the generation it names lost its records, as
    // after a partial settings reset; the payload files alone are not a
    // collection.
    {
        QSettings settings;
        settings.remove(QStringLiteral("catalogSources/%1").arg(committedGeneration));
        settings.sync();
    }

    skygate::ui::tests::LogCapture capture;
    const auto missingRecords = store.loadCatalogCollectionCache();
    QCOMPARE(missingRecords.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Unusable);
    QCOMPARE(
        missingRecords.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::CommittedGenerationMissing
    );
    QVERIFY(missingRecords.diagnostic.contains(QString::number(committedGeneration)));
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("names a generation without stored records")));
}

void SkySettingsStoreTests::failedLaterSourceRawWriteKeepsCommittedSnapshot()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    // Force the update's second source raw sidecar to fail after the first
    // source's new files were already staged.
    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".txt")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].payload != update.sources[0].payload);
}

void SkySettingsStoreTests::failedLaterSourceBinaryWriteKeepsCommittedSnapshot()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".bin")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].binaryPayload != update.sources[0].binaryPayload);
}

void SkySettingsStoreTests::failedMetadataPublicationKeepsCommittedSnapshot()
{
    prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    // Payload staging still succeeds, but the settings write that publishes the
    // new records fails. The committed generation must stay active.
    const QString settingsDirectory = QFileInfo(QSettings().fileName()).absolutePath();
    QVERIFY(QFile::setPermissions(settingsDirectory, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto restorePermissions = qScopeGuard([settingsDirectory]() {
        QFile::setPermissions(
            settingsDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
        );
    });

    // Superuser rights bypass POSIX permission bits, so with them the
    // restrictive mode does not make the settings write fail and this
    // injection would test nothing. Probe the directory after the chmod and
    // skip instead of reporting a failure the injection cannot produce.
    const QString publicationProbePath = QDir(settingsDirectory).filePath(QStringLiteral("publication-probe"));
    QFile publicationProbe(publicationProbePath);
    if (publicationProbe.open(QIODevice::WriteOnly)) {
        publicationProbe.close();
        publicationProbe.remove();
        QSKIP(
            "Restrictive directory permissions do not block writes (running as root); the metadata publication "
            "failure cannot be injected"
        );
    }

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].title != update.sources[0].title);
}

void SkySettingsStoreTests::failedManifestPublicationKeepsCommittedGenerationIntact()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();
    QVERIFY(QFile::remove(manifestPath));
    QVERIFY(QDir().mkpath(manifestPath));

    QVERIFY(!store.saveCatalogCollectionCache(newCollectionSnapshot()));

    // The manifest is replaced atomically, so a failed publication leaves the
    // previous manifest in place. Restoring the recorded bytes shows that the
    // committed generation itself was not touched and its staged successor was
    // discarded.
    QVERIFY(QDir(manifestPath).removeRecursively());
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    const QString stagedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration + 1U, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QVERIFY(!QFile::exists(stagedPath));
}

void SkySettingsStoreTests::failedFirstCollectionPublicationKeepsLegacyCacheReadable()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Legacy custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
    );
    QVERIFY(store.saveCatalogCache(legacy));

    // Block the generation manifest so the first collection save fails after
    // its payload files and records were staged. No generation was durably
    // published, so the readable legacy cache must remain the fallback.
    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QVERIFY(QDir().mkpath(manifestPath));

    const auto firstCollection = makeCollectionSnapshot({makeCollectionRecord(
        QStringLiteral("custom:alpha"), QStringLiteral("Alpha"), QByteArray("raw alpha"), QByteArray("binary alpha")
    )});
    QVERIFY(!store.saveCatalogCollectionCache(firstCollection));
    const auto failedFirstSave = store.loadCatalogCollectionCache();
    QCOMPARE(failedFirstSave.state, SkySettingsStore::CatalogCollectionCacheLoadResult::State::Absent);
    QCOMPARE(failedFirstSave.failure, SkySettingsStore::CatalogCollectionCacheLoadResult::Failure::None);
    // A failed first publication leaves no commit record behind either.
    {
        QSettings settings;
        QVERIFY(!settings.contains(QStringLiteral("skyContext/catalogCollectionCommittedGeneration")));
    }

    const auto legacyAfterFailure = store.loadCatalogCache();
    QVERIFY(legacyAfterFailure.has_value());
    QCOMPARE(legacyAfterFailure->sourceLabel, legacy.sourceLabel);
    QCOMPARE(legacyAfterFailure->catalogPayload, legacy.catalogPayload);
    QCOMPARE(legacyAfterFailure->deepSkyCatalogPayload, legacy.deepSkyCatalogPayload);

    // Once the manifest can be published, the same snapshot commits while the
    // legacy cache stays readable without being the fallback.
    QVERIFY(QDir(manifestPath).removeRecursively());
    QVERIFY(store.saveCatalogCollectionCache(firstCollection));
    const auto committed = store.loadCatalogCollectionCache();
    QVERIFY(committed.isLoaded());
    QCOMPARE(committed.snapshot.sources.size(), 1);
    QCOMPARE(committed.snapshot.sources[0].title, QString("Alpha"));
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkySettingsStoreTests::successfulCollectionUpdateReplacesCommittedGeneration()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const auto update = newCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, update);

    // The committed snapshot is exactly the new generation: its payloads are
    // present and the previous generation's sidecars were removed.
    const QString currentRawPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration + 1U, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QVERIFY(QFile::exists(currentRawPath));
    const QString obsoleteBinaryPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration, QStringLiteral("custom:beta"), QStringLiteral(".bin")
    );
    QVERIFY(!QFile::exists(obsoleteBinaryPath));
}

void SkySettingsStoreTests::failedCollectionSaveLogsExplicitError()
{
    const QString directory = prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));

    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".bin")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    skygate::ui::tests::LogCapture capture(QtInfoMsg);
    QVERIFY(!store.saveCatalogCollectionCache(newCollectionSnapshot()));

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Failed to stage catalog collection binary payload")));
    QVERIFY(messages.contains(QStringLiteral("committed collection remains active")));
    QVERIFY(!messages.contains(QStringLiteral("Catalog collection cache saved")));
}

void SkySettingsStoreTests::partialCatalogCacheSavePreservesConfiguredPeerPath()
{
    const QString starCachePath = m_settings.filePath(QStringLiteral("partial-star-cache.txt"));
    const QString deepSkyCachePath = m_settings.filePath(QStringLiteral("partial-deep-sky-cache.txt"));
    m_settings.clearSettings();
    m_settings.setCatalogCachePaths(starCachePath, deepSkyCachePath);
    QSettings settings;

    SkySettingsStore store;
    SkySettingsStore::CatalogCacheSnapshot starSnapshot;
    starSnapshot.sourceLabel = "Saved";
    starSnapshot.catalogPayload = skygate::ui::tests::sampleHygCsvPayload();
    QVERIFY(store.saveCatalogCache(starSnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);

    SkySettingsStore::CatalogCacheSnapshot deepSkySnapshot;
    deepSkySnapshot.deepSkySourceLabel = "Saved OpenNGC";
    deepSkySnapshot.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    QVERIFY(store.saveCatalogCache(deepSkySnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);
    QVERIFY(QFileInfo::exists(deepSkyCachePath));
}

void SkySettingsStoreTests::missingCacheFilesAndMalformedCacheMetadataAreTolerated()
{
    m_settings.clearSettings();
    m_settings.setCatalogCachePaths(
        m_settings.filePath(QStringLiteral("missing-star-cache.csv")),
        m_settings.filePath(QStringLiteral("missing-deep-sky-cache.csv"))
    );
    QSettings settings;
    settings.setValue("skyContext/catalogSourceLabel", "Missing HYG");
    settings.setValue("skyContext/catalogConstellationLineSchemaVersion", "latest");
    settings.setValue("skyContext/catalogConstellationCount", "many");

    const SkySettingsStore store;
    QVERIFY(!store.loadCatalogCache().has_value());

    const QString cachePath = m_settings.filePath(QStringLiteral("malformed-metadata-cache.csv"));
    QFile cacheFile(cachePath);
    QVERIFY(cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(cacheFile.write(skygate::ui::tests::sampleHygCsvPayload()) > 0);
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

void SkySettingsStoreTests::savesLoadsAndClearsEphemerisDataCacheMetadata()
{
    m_settings.resetSettingsWithCatalogCachePaths(
        QStringLiteral("ephemeris-clear-star-cache.csv"), QStringLiteral("ephemeris-clear-deep-sky-cache.csv")
    );

    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot catalogSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(store.saveCatalogCache(catalogSnapshot));

    SkySettingsStore::EphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    savedSnapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    savedSnapshot.installedKernelPath = m_settings.filePath(QStringLiteral("de440s.bsp"));
    savedSnapshot.installedKernelVersion = QStringLiteral("DE440s-2026a");
    savedSnapshot.installedEarthOrientationPath = m_settings.filePath(QStringLiteral("eop.csv"));
    savedSnapshot.installedEarthOrientationVersion = QStringLiteral("IERS-2026-05");
    savedSnapshot.installedLeapSecondTablePath = m_settings.filePath(QStringLiteral("leap-seconds.list"));
    savedSnapshot.installedLeapSecondTableVersion = QStringLiteral("leap-seconds-2025");
    savedSnapshot.installedDeltaTDataPath = m_settings.filePath(QStringLiteral("delta-t.csv"));
    savedSnapshot.installedDeltaTDataVersion = QStringLiteral("delta-t-2026");
    savedSnapshot.dataRevisionToken = QStringLiteral("ephemeris-rev-42");
    savedSnapshot.lastUpdateResult = QStringLiteral("Updated");

    QVERIFY(store.saveEphemerisDataCache(savedSnapshot));
    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(loadedSnapshot.installedKernelAssetId, savedSnapshot.installedKernelAssetId);
    QCOMPARE(loadedSnapshot.installedKernelProfileId, savedSnapshot.installedKernelProfileId);
    QCOMPARE(loadedSnapshot.installedKernelPath, savedSnapshot.installedKernelPath);
    QCOMPARE(loadedSnapshot.installedKernelVersion, savedSnapshot.installedKernelVersion);
    QCOMPARE(loadedSnapshot.installedEarthOrientationPath, savedSnapshot.installedEarthOrientationPath);
    QCOMPARE(loadedSnapshot.installedEarthOrientationVersion, savedSnapshot.installedEarthOrientationVersion);
    QCOMPARE(loadedSnapshot.installedLeapSecondTablePath, savedSnapshot.installedLeapSecondTablePath);
    QCOMPARE(loadedSnapshot.installedLeapSecondTableVersion, savedSnapshot.installedLeapSecondTableVersion);
    QCOMPARE(loadedSnapshot.installedDeltaTDataPath, savedSnapshot.installedDeltaTDataPath);
    QCOMPARE(loadedSnapshot.installedDeltaTDataVersion, savedSnapshot.installedDeltaTDataVersion);
    QCOMPARE(loadedSnapshot.dataRevisionToken, savedSnapshot.dataRevisionToken);
    QCOMPARE(loadedSnapshot.lastUpdateResult, savedSnapshot.lastUpdateResult);

    QVERIFY(store.clearEphemerisDataCache());
    const auto clearedSnapshot = store.loadEphemerisDataCache();
    QVERIFY(clearedSnapshot.installedKernelAssetId.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelProfileId.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelPath.isEmpty());
    QVERIFY(clearedSnapshot.installedKernelVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedEarthOrientationPath.isEmpty());
    QVERIFY(clearedSnapshot.installedEarthOrientationVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedLeapSecondTablePath.isEmpty());
    QVERIFY(clearedSnapshot.installedLeapSecondTableVersion.isEmpty());
    QVERIFY(clearedSnapshot.installedDeltaTDataPath.isEmpty());
    QVERIFY(clearedSnapshot.installedDeltaTDataVersion.isEmpty());
    QCOMPARE(clearedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(clearedSnapshot.lastUpdateResult, QString("Bundled fallback"));

    const auto stillLoadedCatalogSnapshot = store.loadCatalogCache();
    QVERIFY(stillLoadedCatalogSnapshot.has_value());
    QCOMPARE(stillLoadedCatalogSnapshot->catalogPayload, catalogSnapshot.catalogPayload);
    QCOMPARE(stillLoadedCatalogSnapshot->deepSkyCatalogPayload, catalogSnapshot.deepSkyCatalogPayload);
}

void SkySettingsStoreTests::ephemerisDataCachePathsRoundTripExactly()
{
    QSettings settings;
    settings.clear();

    SkySettingsStore::EphemerisDataCacheSnapshot savedSnapshot;
    savedSnapshot.installedKernelPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced kernel.bsp")));
    savedSnapshot.installedEarthOrientationPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced eop.csv")));
    savedSnapshot.installedLeapSecondTablePath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced leap-seconds.list")));
    savedSnapshot.installedDeltaTDataPath =
        QStringLiteral("  %1  ").arg(m_settings.filePath(QStringLiteral("spaced delta-t.csv")));

    const SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(savedSnapshot));

    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QCOMPARE(loadedSnapshot.installedKernelPath, savedSnapshot.installedKernelPath);
    QCOMPARE(loadedSnapshot.installedEarthOrientationPath, savedSnapshot.installedEarthOrientationPath);
    QCOMPARE(loadedSnapshot.installedLeapSecondTablePath, savedSnapshot.installedLeapSecondTablePath);
    QCOMPARE(loadedSnapshot.installedDeltaTDataPath, savedSnapshot.installedDeltaTDataPath);
}

void SkySettingsStoreTests::partialAndMalformedEphemerisDataCacheMetadataFallsBack()
{
    QSettings settings;
    settings.clear();
    settings.setValue("skyContext/ephemerisData/installedKernelVersion", "DE440s-2026a");
    settings.setValue("skyContext/ephemerisData/dataRevisionToken", "   ");
    settings.setValue("skyContext/ephemerisData/lastUpdateResult", "");

    const SkySettingsStore store;
    const auto loadedSnapshot = store.loadEphemerisDataCache();
    QVERIFY(loadedSnapshot.installedKernelPath.isEmpty());
    QCOMPARE(loadedSnapshot.installedKernelVersion, QString("DE440s-2026a"));
    QVERIFY(loadedSnapshot.installedEarthOrientationPath.isEmpty());
    QVERIFY(loadedSnapshot.installedEarthOrientationVersion.isEmpty());
    QVERIFY(loadedSnapshot.installedLeapSecondTablePath.isEmpty());
    QVERIFY(loadedSnapshot.installedLeapSecondTableVersion.isEmpty());
    QVERIFY(loadedSnapshot.installedDeltaTDataPath.isEmpty());
    QVERIFY(loadedSnapshot.installedDeltaTDataVersion.isEmpty());
    QCOMPARE(loadedSnapshot.dataRevisionToken, QString("bundled"));
    QCOMPARE(loadedSnapshot.lastUpdateResult, QString("Bundled fallback"));
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
    ignoreSkySettingsFallbackWarnings(8);
    const auto loadedSnapshot = store.loadState();
    QVERIFY(loadedSnapshot.has_value());
    const SkySettingsStore::EphemerisUserSettingsSnapshot defaults;
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
    SkySettingsStore::StateSnapshot snapshot;
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
