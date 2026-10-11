#include "CatalogArchiveFixture.hpp"
#include "CatalogBodyQuery.hpp"
#include "CatalogCacheTestSupport.hpp"
#include "CatalogSourceFactsQuery.hpp"
#include "CatalogTestPayloadFile.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using skygate::ui::tests::CatalogArchiveFixture;
using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::CatalogTestPayloadFile;

namespace {

void compareConfiguredSourceFields(
    const SkySettingsStore::CatalogSourceCacheRecord& actual, const SkySettingsStore::CatalogSourceCacheRecord& expected
)
{
    QCOMPARE(actual.instanceId, expected.instanceId);
    QCOMPARE(actual.descriptorId, expected.descriptorId);
    QCOMPARE(actual.title, expected.title);
    QCOMPARE(actual.version, expected.version);
    QCOMPARE(actual.url, expected.url);
    QCOMPARE(actual.urls, expected.urls);
    QCOMPARE(actual.relatedDatasetUrls, expected.relatedDatasetUrls);
    QCOMPARE(actual.archiveSelector, expected.archiveSelector);
    QCOMPARE(actual.schemaHint, expected.schemaHint);
    QCOMPARE(actual.attribution, expected.attribution);
    QCOMPARE(actual.policy, expected.policy);
    QCOMPARE(actual.enabled, expected.enabled);
    QCOMPARE(actual.bundled, expected.bundled);
    QCOMPARE(actual.order, expected.order);
}

}  // namespace

class SkyCatalogManagerPersistenceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoreCachePathThroughManager();
    void clearSourceCacheKeepsConfiguredRecords();
    void restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss();
    void persistsBundledSourceConfigurationWithoutNetworkOperation();
    void restoredBundledDeepSkySourceKeepsFreshObjectCount();
    void interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence();
    void rejectedRestoreKeepsPreviousCollectionAndReportsError();
    void removedBundledSourceDoesNotReturnAfterRestart();
    void savedEmptyCollectionRestoresEmptyAcrossRestarts();

private:
    SkySettingsStore::CatalogCacheSnapshot makeCacheSnapshot() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerPersistenceTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerPersistenceTests")));
}

void SkyCatalogManagerPersistenceTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogManagerPersistenceTests::makeCacheSnapshot() const
{
    return skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
}

void SkyCatalogManagerPersistenceTests::restoreCachePathThroughManager()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(makeCacheSnapshot()));

    SkyCatalogManager manager(&store);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    const QString starUrl = QStringLiteral("https://example.test/custom-stars.csv");
    const QString deepSkyUrl = QStringLiteral("https://example.test/custom-dso.csv");
    manager.setCatalogPresetIndex(2);
    manager.setCatalogUrlText(starUrl);
    manager.setDeepSkyCatalogPresetIndex(2);
    manager.setDeepSkyCatalogUrlText(deepSkyUrl);

    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceLabel(), QString("Custom (saved)"));
    QCOMPARE(manager.constellationCount(), 1U);
    QCOMPARE(manager.constellationLineRefs().size(), 1U);
    QVERIFY(manager.bodyCount() > 0U);
    QCOMPARE(catalogSpy.count(), 1);
}

void SkyCatalogManagerPersistenceTests::clearSourceCacheKeepsConfiguredRecords()
{
    const QString aPath = m_settings.filePath(QStringLiteral("clear-a.csv"));
    const QString bPath = m_settings.filePath(QStringLiteral("clear-b.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            aPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 902030, .hip = 902030, .properName = "Clear Star A", .mag = "1.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            bPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 902031, .hip = 902031, .properName = "Clear Star B", .mag = "1.0"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const QString aUrl = QUrl::fromLocalFile(aPath).toString();
    const QString bUrl = QUrl::fromLocalFile(bPath).toString();

    manager.addSourceUrl(aUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString aId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(bUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString bId = manager.sourceInstanceIds().last();

    const auto beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, aId);
    QVERIFY(recordBeforeClear != nullptr);
    QVERIFY(!recordBeforeClear->payload.isEmpty());

    QVERIFY(manager.clearSourceCache(aId));

    // The accepted in-memory snapshot stays active for the session, so the
    // cleared source keeps contributing until a reload or restart.
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), aId, bId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Clear Star A")));

    // Only the cleared source's disposable payload is evicted; both configured
    // records stay and the cleared record keeps every configuration field.
    const auto clearedCache = store.loadCatalogCollectionCache();
    QVERIFY(clearedCache.isLoaded());
    QCOMPARE(clearedCache.snapshot.sources.size(), 3);
    const SkySettingsStore::CatalogSourceCacheRecord* clearedRecord =
        CatalogSourceFactsQuery::findRecord(clearedCache.snapshot, aId);
    QVERIFY(clearedRecord != nullptr);
    compareConfiguredSourceFields(*clearedRecord, *recordBeforeClear);
    QVERIFY(clearedRecord->payload.isEmpty());
    QVERIFY(clearedRecord->binaryPayload.isEmpty());
    QVERIFY(clearedRecord->constellationLineRows.isEmpty());
    QVERIFY(clearedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(clearedRecord->constellationCount, std::size_t{0});
    const SkySettingsStore::CatalogSourceCacheRecord* peerRecord =
        CatalogSourceFactsQuery::findRecord(clearedCache.snapshot, bId);
    QVERIFY(peerRecord != nullptr);
    QVERIFY(!peerRecord->payload.isEmpty());
    QVERIFY(!peerRecord->binaryPayload.isEmpty());

    // A metadata-only persistence of the surviving collection keeps the
    // eviction instead of re-serializing the retained in-memory payload.
    manager.moveSource(bId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
    const auto reordered = store.loadCatalogCollectionCache();
    QVERIFY(reordered.isLoaded());
    QCOMPARE(reordered.snapshot.sources.size(), 3);
    QCOMPARE(reordered.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QCOMPARE(reordered.snapshot.sources[1].instanceId, bId);
    QCOMPARE(reordered.snapshot.sources[2].instanceId, aId);
    QVERIFY(!reordered.snapshot.sources[1].payload.isEmpty());
    const SkySettingsStore::CatalogSourceCacheRecord* reorderedCleared =
        CatalogSourceFactsQuery::findRecord(reordered.snapshot, aId);
    QVERIFY(reorderedCleared != nullptr);
    SkySettingsStore::CatalogSourceCacheRecord expectedReordered = *recordBeforeClear;
    expectedReordered.order = 2;
    compareConfiguredSourceFields(*reorderedCleared, expectedReordered);
    QVERIFY(reorderedCleared->payload.isEmpty());
    QVERIFY(reorderedCleared->binaryPayload.isEmpty());

    // Restart: the evicted source stays configured and reports the unavailable
    // payload and retry state, while the peer restores its payload normally.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Saved catalog source cache has no payload")));
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star B")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star A")));
    const QVector<SkyCatalogManager::SourceViewEntry> view = restarted.sourceViewEntries();
    const auto evictedEntry =
        std::find_if(view.begin(), view.end(), [&aId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == aId;
        });
    QVERIFY(evictedEntry != view.end());
    QVERIFY(evictedEntry->hasError);
    QVERIFY(!evictedEntry->busy);
    QCOMPARE(evictedEntry->statusText, QStringLiteral("Payload unavailable"));

    // An explicit successful reload repopulates the payload and restores the
    // source data; the eviction is not permanent configuration.
    restarted.retrySource(aId);
    QTRY_VERIFY(!restarted.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star A")));
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recoveredRecord =
        CatalogSourceFactsQuery::findRecord(recovered.snapshot, aId);
    QVERIFY(recoveredRecord != nullptr);
    QVERIFY(!recoveredRecord->payload.isEmpty());

    // Remove stays the separate configuration-deletion operation.
    restarted.removeSource(aId);
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId}));
    const auto afterRemove = store.loadCatalogCollectionCache();
    QVERIFY(afterRemove.isLoaded());
    QCOMPARE(afterRemove.snapshot.sources.size(), 2);
    QVERIFY(CatalogSourceFactsQuery::findRecord(afterRemove.snapshot, aId) == nullptr);
}

void SkyCatalogManagerPersistenceTests::restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("archive-restore-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString archiveUrl = QStringLiteral("https://example.test/members.zip");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = CatalogArchiveFixture::archiveMemberZip(
             {.name = "NGC0991", .messier = "", .ngc = "0991", .identifiers = "PGC 9991", .commonName = "First"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(archiveUrl, QStringLiteral("v2026.1"));
    const QString instanceId = source.instanceId;
    source.descriptorId = QStringLiteral("archive_demo");
    source.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    source.archiveSelector = CatalogArchiveFixture::deepSkyMemberPath();
    source.attribution = QStringLiteral("Demo archive attribution");

    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("hip_900101")));

    // The parse contract and descriptor metadata are persisted with the source.
    // The default bundled source is persisted as configuration without a
    // payload so its position survives the restart.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QCOMPARE(persisted.snapshot.sources.size(), 2);
    const auto persistedSource = std::find_if(
        persisted.snapshot.sources.begin(),
        persisted.snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(persistedSource != persisted.snapshot.sources.end());
    QCOMPARE(persistedSource->descriptorId, QString("archive_demo"));
    QCOMPARE(persistedSource->version, QString("v2026.1"));
    QCOMPARE(persistedSource->archiveSelector, CatalogArchiveFixture::deepSkyMemberPath());
    QCOMPARE(persistedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(persistedSource->attribution, QString("Demo archive attribution"));

    // Losing the binary sidecar must not lose the selected archive member.
    const QDir cacheDir(cacheDirectory);
    const QStringList binaryFiles =
        cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*.bin")}, QDir::Files);
    QCOMPARE(binaryFiles.size(), 1);
    const QString binaryPath = CatalogSourceFactsQuery::sidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    QCOMPARE(binaryFiles.first(), QFileInfo(binaryPath).fileName());
    QVERIFY(QFile::remove(binaryPath));

    SkyCatalogManager restoredManager(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    QVERIFY(CatalogBodyQuery::containsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!CatalogBodyQuery::containsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));

    // The raw-payload fallback rewrote the upgraded record with the same parse
    // contract, and a reload keeps replaying the restored selection.
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.isLoaded());
    QCOMPARE(upgraded.snapshot.sources.size(), 2);
    const auto upgradedSource = std::find_if(
        upgraded.snapshot.sources.begin(),
        upgraded.snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(upgradedSource != upgraded.snapshot.sources.end());
    QCOMPARE(upgradedSource->archiveSelector, CatalogArchiveFixture::deepSkyMemberPath());
    QCOMPARE(upgradedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(upgradedSource->attribution, QString("Demo archive attribution"));

    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = CatalogArchiveFixture::archiveMemberZip(
             {.name = "NGC0993", .messier = "", .ngc = "0993", .identifiers = "PGC 9993", .commonName = "Reloaded"}
         )}
    );
    restoredManager.retrySource(instanceId);
    QTRY_VERIFY(!restoredManager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsId(restoredManager.starCatalog(), QStringLiteral("ngc_993")));
    QVERIFY(!CatalogBodyQuery::containsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!CatalogBodyQuery::containsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));
}

void SkyCatalogManagerPersistenceTests::persistsBundledSourceConfigurationWithoutNetworkOperation()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});
    const QString bundledId = manager.sourceInstanceIds().first();
    QCOMPARE(bundledId, QStringLiteral("primary"));
    QVERIFY(manager.isSourceEnabled(bundledId));

    // Disabling the bundled source persists its configuration alone; no URL,
    // download, or operation record is involved.
    manager.disableSource(bundledId);

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 1);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, bundledId);
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!snapshot.snapshot.sources[0].enabled);
    QVERIFY(snapshot.snapshot.sources[0].urls.isEmpty());
    QVERIFY(snapshot.snapshot.sources[0].payload.isEmpty());
    QVERIFY(snapshot.snapshot.sources[0].binaryPayload.isEmpty());

    // Restart rebuilds the bundled source from the factory and keeps it
    // disabled instead of silently enabling it again.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{bundledId});
    QVERIFY(!restoredManager.isSourceEnabled(bundledId));
    QVERIFY(restoredManager.starCatalog() != nullptr);
    const auto composedIds = restoredManager.sourceIds();
    QVERIFY(std::none_of(composedIds.begin(), composedIds.end(), [&bundledId](const QString& sourceId) {
        return sourceId == bundledId;
    }));

    const QVector<SkyCatalogManager::SourceViewEntry> view = restoredManager.sourceViewEntries();
    QCOMPARE(view.size(), 1);
    QCOMPARE(view[0].instanceId, bundledId);
    QCOMPARE(view[0].title, QStringLiteral("Bundled"));
    QVERIFY(view[0].bundled);
    QVERIFY(!view[0].enabled);
}

void SkyCatalogManagerPersistenceTests::restoredBundledDeepSkySourceKeepsFreshObjectCount()
{
    SkySettingsStore store;
    QString freshInfoText;
    {
        SkyCatalogManager manager(&store);
        manager.addSourcePreset(QStringLiteral("bundled_messier"));
        freshInfoText = manager.deepSkyCatalogInfoText();
        QVERIFY(freshInfoText.contains(QStringLiteral("Objects:")));
    }

    // A restart reports the same deep-sky object count as the fresh add: the
    // restored bundled record leaves the count to the bundled fallback
    // participation instead of reporting the rebuilt catalog's deep-sky
    // objects a second time.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.deepSkyCatalogInfoText(), freshInfoText);
}

void SkyCatalogManagerPersistenceTests::interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!CatalogTestPayloadFile::write(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = 906001, .hip = 906001, .properName = QByteArray(properName), .mag = "1.0"}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    const QString sourceAUrl = writeSourceFile(QStringLiteral("interleave-a.csv"), "Interleave Star A");
    const QString sourceBUrl = writeSourceFile(QStringLiteral("interleave-b.csv"), "Interleave Star B");
    QVERIFY(!sourceAUrl.isEmpty());
    QVERIFY(!sourceBUrl.isEmpty());

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    manager.addSourceUrl(sourceAUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString sourceAId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(sourceBUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString sourceBId = manager.sourceInstanceIds().last();
    manager.addSourcePreset(QStringLiteral("bundled_messier"));
    const QString bundledDeepSkyId = manager.sourceInstanceIds().last();
    QCOMPARE(manager.sourceCount(), std::size_t{4});

    // Interleave the bundled source between the two downloaded sources so the
    // collection is neither bundled-first nor bundled-last.
    manager.moveSource(QStringLiteral("primary"), 1);
    QCOMPARE(
        manager.sourceInstanceIds(), QStringList({sourceAId, QStringLiteral("primary"), sourceBId, bundledDeepSkyId})
    );
    manager.disableSource(bundledDeepSkyId);

    // The later Merge source keeps its precedence for the shared identity.
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star A")));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({sourceAId, QStringLiteral("primary"), sourceBId, bundledDeepSkyId})
    );

    const QVector<SkyCatalogManager::SourceViewEntry> view = restoredManager.sourceViewEntries();
    const auto findEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto sourceAEntry = findEntry(sourceAId);
    const auto bundledEntry = findEntry(QStringLiteral("primary"));
    const auto sourceBEntry = findEntry(sourceBId);
    const auto bundledDeepSkyEntry = findEntry(bundledDeepSkyId);
    QVERIFY(sourceAEntry != view.end());
    QVERIFY(bundledEntry != view.end());
    QVERIFY(sourceBEntry != view.end());
    QVERIFY(bundledDeepSkyEntry != view.end());
    QCOMPARE(sourceAEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!sourceAEntry->bundled);
    QVERIFY(sourceAEntry->enabled);
    QCOMPARE(bundledEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(bundledEntry->bundled);
    QVERIFY(bundledEntry->enabled);
    QCOMPARE(bundledEntry->title, QStringLiteral("Bundled"));
    QCOMPARE(sourceBEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!sourceBEntry->bundled);
    QVERIFY(sourceBEntry->enabled);
    QCOMPARE(bundledDeepSkyEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(bundledDeepSkyEntry->bundled);
    QVERIFY(!bundledDeepSkyEntry->enabled);
    QCOMPARE(bundledDeepSkyEntry->title, QStringLiteral("Bundled Messier"));

    // Restored order keeps the resulting precedence.
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star A")));
}

void SkyCatalogManagerPersistenceTests::rejectedRestoreKeepsPreviousCollectionAndReportsError()
{
    // A persisted instance identity colliding with the implicit bundled core
    // source makes the restored collection uncomposable. The rejected restore
    // must report the operation error and keep the previous configuration,
    // snapshot, source rows, and stored cache instead of half-applying the
    // staged collection.
    const QString collidingUrl = QStringLiteral("https://example.test/colliding-stars.csv");

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

    SkySettingsStore::CatalogSourceCacheRecord colliding;
    colliding.instanceId = QStringLiteral("bundled-core");
    colliding.title = QStringLiteral("Colliding Source");
    colliding.urls = QStringList{collidingUrl};
    colliding.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    colliding.enabled = true;
    colliding.order = 0;
    colliding.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Colliding Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(colliding));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    SkyCatalogManager manager(&store);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const QStringList sourcesBefore = manager.sourceInstanceIds();
    const std::size_t bodyCountBefore = manager.bodyCount();
    const std::uint64_t revisionBefore = manager.catalogRevision();
    const int sourceRowCountBefore = manager.sourceViewEntries().size();

    QVERIFY(!manager.restoreCatalogCache());

    // The operation error is reported through the normal status signal, and
    // the failed transition publishes no catalog or dataset change.
    QCOMPARE(statusSpy.count(), 1);
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Collection rejected")));
    QCOMPARE(catalogSpy.count(), 0);
    QCOMPARE(datasetSpy.count(), 0);

    QCOMPARE(manager.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(manager.bodyCount(), bodyCountBefore);
    QCOMPARE(manager.catalogRevision(), revisionBefore);
    QVERIFY(manager.starCatalog() != nullptr);

    // No staged row appears for the rejected collection.
    const QVector<SkyCatalogManager::SourceViewEntry> rows = manager.sourceViewEntries();
    QCOMPARE(rows.size(), sourceRowCountBefore);
    QCOMPARE(rows.first().instanceId, QStringLiteral("primary"));
    QVERIFY(!rows.first().hasError);

    // The rejected collection is not persisted in place of the stored cache.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(CatalogSourceFactsQuery::containsInstanceId(persisted.snapshot, QStringLiteral("bundled-core")));
    QVERIFY(!CatalogSourceFactsQuery::containsInstanceId(persisted.snapshot, QStringLiteral("primary")));
}

void SkyCatalogManagerPersistenceTests::removedBundledSourceDoesNotReturnAfterRestart()
{
    const QString siblingPath = m_settings.filePath(QStringLiteral("removed-bundled-sibling.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            siblingPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 906020, .hip = 906020, .properName = "Sibling Star", .mag = "1.0"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    manager.addSourceUrl(QUrl::fromLocalFile(siblingPath).toString(), QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString siblingId = manager.sourceInstanceIds().last();

    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.sourceInstanceIds(), QStringList{siblingId});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 1);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, siblingId);

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{siblingId});
    const auto composedIds = restoredManager.sourceIds();
    QVERIFY(std::none_of(composedIds.begin(), composedIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("primary");
    }));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Sibling Star")));
}

void SkyCatalogManagerPersistenceTests::savedEmptyCollectionRestoresEmptyAcrossRestarts()
{
    SkySettingsStore store;
    {
        SkyCatalogManager manager(&store);
        QCOMPARE(manager.sourceCount(), std::size_t{1});
        manager.removeSource(QStringLiteral("primary"));
        QCOMPARE(manager.sourceCount(), std::size_t{0});

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // Restart 1: the stored empty snapshot restores successfully and stays
    // empty; the first-use default is not reinstated over it.
    {
        SkyCatalogManager firstRestart(&store);
        QSignalSpy sourcesSpy(&firstRestart, &SkyCatalogManager::sourcesChanged);
        QSignalSpy catalogSpy(&firstRestart, &SkyCatalogManager::catalogChanged);
        QVERIFY(firstRestart.restoreCatalogCache());
        QCOMPARE(firstRestart.sourceCount(), std::size_t{0});
        QVERIFY(firstRestart.sourceInstanceIds().isEmpty());
        QVERIFY(firstRestart.sourceViewEntries().isEmpty());
        QCOMPARE(firstRestart.participationSummary(), QStringLiteral("Bundled core + Bundled Messier"));
        QVERIFY(catalogSpy.count() >= 1);
        QVERIFY(sourcesSpy.count() >= 1);

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // Restart 2: the configuration is still empty and no default source has
    // crept back into the stored snapshot.
    {
        SkyCatalogManager secondRestart(&store);
        QVERIFY(secondRestart.restoreCatalogCache());
        QCOMPARE(secondRestart.sourceCount(), std::size_t{0});
        QVERIFY(secondRestart.sourceInstanceIds().isEmpty());
        QVERIFY(secondRestart.sourceViewEntries().isEmpty());

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }
}

QTEST_GUILESS_MAIN(SkyCatalogManagerPersistenceTests)

#include "SkyCatalogManagerPersistenceTests.moc"
