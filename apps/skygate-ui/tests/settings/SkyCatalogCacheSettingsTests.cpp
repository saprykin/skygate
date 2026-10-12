#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/CatalogCollectionCacheFixture.hpp"
#include "support/CatalogTestPayloads.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>

using skygate::ui::tests::CatalogCollectionCacheFixture;

namespace {

void ignoreSkySettingsFallbackWarnings(const int count)
{
    for (int index = 0; index < count; ++index) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid .*setting skyContext/.* - using fallback .*"));
    }
}

}  // namespace

class SkyCatalogCacheSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void savesLoadsAndClearsCatalogCachesIndependently();
    void savesLoadsAndClearsCatalogCollectionCache();
    void clearSourceCacheEvictsPayloadKeepsConfiguration();
    void unreadablePayloadIsReportedDistinctFromEviction();
    void partialCacheSavePreservesConfiguredPeerPath();
    void missingCacheFilesAndMalformedMetadataTolerated();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheSettingsTests")));
}

void SkyCatalogCacheSettingsTests::savesLoadsAndClearsCatalogCachesIndependently()
{
    m_settings.resetSettingsWithCatalogCachePaths(
        QStringLiteral("catalog-cache-test.txt"), QStringLiteral("deep-sky-catalog-cache-test.txt")
    );

    SkySettingsStore store;
    const SkyCatalogLegacyCacheSnapshot savedSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot(
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

void SkyCatalogCacheSettingsTests::savesLoadsAndClearsCatalogCollectionCache()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();

    SkySettingsStore store;
    const auto savedSnapshot = CatalogCollectionCacheFixture::sampleCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(savedSnapshot));

    const SkyCatalogCollectionCacheLoadResult loaded = store.loadCatalogCollectionCache();
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
    QCOMPARE(afterClear.state, SkyCatalogCollectionCacheLoadResult::State::Absent);
}

void SkyCatalogCacheSettingsTests::clearSourceCacheEvictsPayloadKeepsConfiguration()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto savedSnapshot = CatalogCollectionCacheFixture::sampleCollectionSnapshot();
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

void SkyCatalogCacheSettingsTests::unreadablePayloadIsReportedDistinctFromEviction()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto savedSnapshot = CatalogCollectionCacheFixture::sampleCollectionSnapshot();
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

void SkyCatalogCacheSettingsTests::partialCacheSavePreservesConfiguredPeerPath()
{
    const QString starCachePath = m_settings.filePath(QStringLiteral("partial-star-cache.txt"));
    const QString deepSkyCachePath = m_settings.filePath(QStringLiteral("partial-deep-sky-cache.txt"));
    m_settings.clearSettings();
    m_settings.setCatalogCachePaths(starCachePath, deepSkyCachePath);
    QSettings settings;

    SkySettingsStore store;
    SkyCatalogLegacyCacheSnapshot starSnapshot;
    starSnapshot.sourceLabel = "Saved";
    starSnapshot.catalogPayload = skygate::ui::tests::sampleHygCsvPayload();
    QVERIFY(store.saveCatalogCache(starSnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);

    SkyCatalogLegacyCacheSnapshot deepSkySnapshot;
    deepSkySnapshot.deepSkySourceLabel = "Saved OpenNGC";
    deepSkySnapshot.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    QVERIFY(store.saveCatalogCache(deepSkySnapshot));
    QCOMPARE(settings.value("skyContext/catalogCachePath").toString(), starCachePath);
    QCOMPARE(settings.value("skyContext/deepSkyCatalogCachePath").toString(), deepSkyCachePath);
    QVERIFY(QFileInfo::exists(deepSkyCachePath));
}

void SkyCatalogCacheSettingsTests::missingCacheFilesAndMalformedMetadataTolerated()
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

QTEST_GUILESS_MAIN(SkyCatalogCacheSettingsTests)

#include "SkyCatalogCacheSettingsTests.moc"
