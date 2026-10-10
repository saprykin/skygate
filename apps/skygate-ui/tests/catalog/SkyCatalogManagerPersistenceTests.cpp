#include "CatalogArchiveFixture.hpp"
#include "CatalogBodyQuery.hpp"
#include "CatalogCacheTestSupport.hpp"
#include "CatalogSourceFactsQuery.hpp"
#include "CatalogTestPayloadFile.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationAnchorGroupLookup.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "FakeReplyQuery.hpp"
#include "RelatedDatasetFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QThreadPool>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using skygate::ui::tests::CatalogArchiveFixture;
using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::CatalogTestPayloadFile;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

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
    void restoresLegacyPersistedInstanceIdsWithReferences();
    void legacyMigrationRenamesDuplicateInstanceIds();
    void restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss();
    void legacyBundledStarSlotSurvivesMigrationThroughManager();
    void persistsBundledSourceConfigurationWithoutNetworkOperation();
    void restoredBundledDeepSkySourceKeepsFreshObjectCount();
    void interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence();
    void unreadablePayloadKeepsConfiguredSourceWithoutErasingSiblings();
    void rejectedRestoreKeepsPreviousCollectionAndReportsError();
    void removedBundledSourceDoesNotReturnAfterRestart();
    void legacySourcesStayRetiredAfterMigratedCollectionIsEmptied();
    void interruptedGenerationUpgradeRestoresFlatSourcesAcrossRestarts();
    void unreadableCommittedCollectionKeepsActiveCollectionAndReportsFailure();
    void savedEmptyCollectionRestoresEmptyAcrossRestarts();
    void liveRestoreDropsOperationsOmittedFromTheRestoredCollection();
    void liveRestoreSupersedesPendingImportReply();
    void liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId();
    void rejectedRestoreKeepsPendingImportWork();

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

void SkyCatalogManagerPersistenceTests::restoresLegacyPersistedInstanceIdsWithReferences()
{
    const QString legacyUrl = QStringLiteral("https://example.test/legacy-stars.csv");
    const skygate::ui::internal::SkyCatalogSourceInstance legacyInstance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(legacyUrl);
    const QString legacyInstanceId =
        skygate::ui::internal::SkyCatalogSourceInstance::migratedLegacyInstanceId(legacyInstance);
    QVERIFY(legacyInstanceId.startsWith(QStringLiteral("custom:")));

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = 1;
    SkySettingsStore::CatalogSourceCacheRecord record;
    record.instanceId = legacyInstanceId;
    record.title = QStringLiteral("Legacy Stars");
    record.version = QStringLiteral("v1");
    record.urls = QStringList{legacyUrl};
    record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    record.enabled = true;
    record.order = 0;
    record.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 904001, .hip = 904001, .properName = "Legacy Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(record));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        legacyUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 904002, .hip = 904002, .properName = "Reloaded Legacy Star", .mag = "1.0"}
         )}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(manager.restoreCatalogCache());

    // The legacy ID is adopted verbatim so settings groups, payload sidecars,
    // operations, and provenance keep addressing the same instance.
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));

    QStringList composedSourceIds;
    for (const QString& sourceId : manager.sourceIds()) {
        composedSourceIds.push_back(sourceId);
    }
    QVERIFY(composedSourceIds.contains(legacyInstanceId));
    bool hasLegacyContributor = false;
    for (const QStringList& contributors : manager.contributorSourceIds()) {
        hasLegacyContributor = hasLegacyContributor || contributors.contains(legacyInstanceId);
    }
    QVERIFY(hasLegacyContributor);

    // Operation lookup and reload keep referring to the restored instance ID.
    manager.retrySource(legacyInstanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Legacy Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));

    // Restart keeps the same legacy ID.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({legacyInstanceId}));
}

void SkyCatalogManagerPersistenceTests::legacyMigrationRenamesDuplicateInstanceIds()
{
    const QString sharedUrl = QStringLiteral("https://example.test/shared-slot-catalog.csv");
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Stars");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 905001, .hip = 905001, .properName = "Legacy Slot Star", .mag = "1.0"}
    );
    legacy.deepSkySourceLabel = QStringLiteral("Legacy Deep Sky");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0995",
         .type = "G",
         .ra = "00:40:00.00",
         .dec = "+12:00:00.0",
         .messier = "",
         .ngc = "0995",
         .identifiers = "PGC 9995",
         .commonName = "Legacy Slot Galaxy"}
    );

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    SkyCatalogManager manager(&store);
    manager.setCatalogPresetIndex(2);
    manager.setCatalogUrlText(sharedUrl);
    manager.setDeepSkyCatalogPresetIndex(2);
    manager.setDeepSkyCatalogUrlText(sharedUrl);

    // Both legacy slots name the same URL, so the legacy derivation maps them
    // to one ID. The second record is deterministically renamed instead of
    // overwriting the first or failing the restore.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicate persisted catalog source instance id"));
    QVERIFY(manager.restoreCatalogCache());

    const QStringList restoredIds = manager.sourceInstanceIds();
    QCOMPARE(restoredIds.size(), 2);
    QVERIFY(restoredIds[0].startsWith(QStringLiteral("custom:")));
    QCOMPARE(restoredIds[1], restoredIds[0] + QStringLiteral("#2"));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Slot Star")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 995")));

    // The renamed IDs stay stable through the migration persist and restart.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), restoredIds);
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

void SkyCatalogManagerPersistenceTests::legacyBundledStarSlotSurvivesMigrationThroughManager()
{
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.deepSkySourceLabel = QStringLiteral("OpenNGC");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    // The legacy star slot selected the bundled source. Migration keeps it in
    // first position instead of dropping it once the downloaded deep-sky
    // source is restored.
    SkyCatalogManager manager(&store);
    manager.setDeepSkyCatalogPresetIndex(1);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(
        manager.sourceInstanceIds(), QStringList({QStringLiteral("preset:bundled"), QStringLiteral("preset:open_ngc")})
    );
    QVERIFY(manager.isSourceEnabled(QStringLiteral("preset:bundled")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    QCOMPARE(view.size(), 2);
    QCOMPARE(view[0].instanceId, QStringLiteral("preset:bundled"));
    QVERIFY(view[0].bundled);
    QCOMPARE(view[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(view[1].instanceId, QStringLiteral("preset:open_ngc"));
    QVERIFY(!view[1].bundled);
    QCOMPARE(view[1].policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // The migrated collection is durable: the next start reads the collection
    // snapshot instead of migrating the legacy cache again.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("preset:bundled"), QStringLiteral("preset:open_ngc")})
    );
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

void SkyCatalogManagerPersistenceTests::unreadablePayloadKeepsConfiguredSourceWithoutErasingSiblings()
{
    const QString damagedUrl = QStringLiteral("https://example.test/damaged-stars.csv");
    const QString healthyUrl = QStringLiteral("https://example.test/healthy-stars.csv");

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

    SkySettingsStore::CatalogSourceCacheRecord bundled;
    bundled.instanceId = QStringLiteral("primary");
    bundled.title = QStringLiteral("Bundled");
    bundled.bundled = true;
    bundled.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bundled.enabled = true;
    bundled.order = 0;
    snapshot.sources.push_back(std::move(bundled));

    SkySettingsStore::CatalogSourceCacheRecord damaged;
    damaged.instanceId = QStringLiteral("custom:damaged");
    damaged.title = QStringLiteral("Damaged Source");
    damaged.urls = QStringList{damagedUrl};
    damaged.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    damaged.enabled = true;
    damaged.order = 1;
    damaged.payload = "this is not a catalog";
    snapshot.sources.push_back(std::move(damaged));

    SkySettingsStore::CatalogSourceCacheRecord healthy;
    healthy.instanceId = QStringLiteral("custom:healthy");
    healthy.title = QStringLiteral("Healthy Source");
    healthy.urls = QStringList{healthyUrl};
    healthy.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    healthy.enabled = true;
    healthy.order = 2;
    healthy.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 906010, .hip = 906010, .properName = "Healthy Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(healthy));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        damagedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 906011, .hip = 906011, .properName = "Recovered Star", .mag = "1.0"}
         )}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Saved catalog source cache unreadable; restoring configuration without payload")
    );
    QVERIFY(manager.restoreCatalogCache());

    // The damaged source keeps its identity, order, policy, and participation
    // state next to its successfully restored siblings.
    QCOMPARE(
        manager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(manager.isSourceEnabled(QStringLiteral("custom:damaged")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Unavailable sources")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Healthy Star")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto damagedEntry =
        std::find_if(view.begin(), view.end(), [](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == QStringLiteral("custom:damaged");
        });
    QVERIFY(damagedEntry != view.end());
    QVERIFY(damagedEntry->hasError);
    QCOMPARE(damagedEntry->statusText, QStringLiteral("Payload unavailable"));
    QCOMPARE(damagedEntry->objectCount, std::size_t{0});

    // The upgrade persist keeps the unreadable record and its raw payload for
    // diagnostics instead of erasing the configured source.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QCOMPARE(persisted.snapshot.sources.size(), 3);
    const auto persistedDamaged = std::find_if(
        persisted.snapshot.sources.begin(),
        persisted.snapshot.sources.end(),
        [](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == QStringLiteral("custom:damaged");
        }
    );
    QVERIFY(persistedDamaged != persisted.snapshot.sources.end());
    QCOMPARE(persistedDamaged->urls, QStringList{damagedUrl});
    QCOMPARE(persistedDamaged->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(persistedDamaged->enabled);
    QCOMPARE(persistedDamaged->payload, QByteArray("this is not a catalog"));

    // The preserved configuration is enough to retry the configured download.
    manager.retrySource(QStringLiteral("custom:damaged"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Recovered Star")));

    // A further restart keeps all three configured sources.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Recovered Star")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Healthy Star")));
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

void SkyCatalogManagerPersistenceTests::legacySourcesStayRetiredAfterMigratedCollectionIsEmptied()
{
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Custom");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Legacy Star", .mag = "1.0"}
    );
    legacy.deepSkySourceLabel = QStringLiteral("Legacy OpenNGC");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload(
        {.name = "NGC0707", .messier = "", .ngc = "0707", .identifiers = "PGC 707", .commonName = "Legacy Galaxy"}
    );

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    {
        SkyCatalogManager manager(&store);
        manager.setCatalogPresetIndex(2);
        manager.setDeepSkyCatalogPresetIndex(2);
        QVERIFY(manager.restoreCatalogCache());

        QCOMPARE(manager.sourceCount(), std::size_t{2});
        QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
        QCOMPARE(manager.sourceViewEntries().size(), 2);
        QVERIFY(manager.statusText().contains(QStringLiteral("Legacy")));

        // Removing every migrated source commits the collection boundary; the
        // legacy cache itself stays readable instead of being deleted. The
        // last removal leaves the collection deliberately empty instead of
        // reinstating an implicit default source.
        const QStringList migratedInstanceIds = manager.sourceInstanceIds();
        for (const QString& instanceId : migratedInstanceIds) {
            manager.removeSource(instanceId);
        }
        QVERIFY(manager.sourceInstanceIds().isEmpty());
        QVERIFY(manager.sourceViewEntries().isEmpty());
        QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
        QVERIFY(store.loadCatalogCache().has_value());
        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // The restart reads the committed empty collection instead of resurrecting
    // the still-readable legacy two-slot cache, and the bundled default source
    // is not reinstated.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(restoredManager.sourceInstanceIds().isEmpty());
    QVERIFY(restoredManager.sourceViewEntries().isEmpty());
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!restoredManager.sourceTitles().values().contains(QStringLiteral("Legacy Custom")));
    QVERIFY(!restoredManager.sourceTitles().values().contains(QStringLiteral("Legacy OpenNGC")));
    QVERIFY(restoredManager.participationSummary().startsWith(QStringLiteral("Bundled")));
    QVERIFY(store.loadCatalogCache().has_value());

    // An explicitly empty persisted collection - what remains after every
    // source of a migrated collection was removed - is a committed
    // configuration as well: the restart restores it as an empty collection of
    // zero sources and never falls back to the legacy two-slot cache that is
    // still readable.
    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    SkyCatalogManager emptyCollectionManager(&store);
    QVERIFY(emptyCollectionManager.restoreCatalogCache());
    QVERIFY(emptyCollectionManager.sourceInstanceIds().isEmpty());
    QVERIFY(emptyCollectionManager.sourceViewEntries().isEmpty());
    QVERIFY(
        !CatalogBodyQuery::containsDisplayName(emptyCollectionManager.starCatalog(), QStringLiteral("Legacy Star"))
    );
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy Custom")));
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy OpenNGC")));
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerPersistenceTests::interruptedGenerationUpgradeRestoresFlatSourcesAcrossRestarts()
{
    SkySettingsStore store;
    // The retired two-slot cache stays readable on disk, so a restore that
    // migrated it instead of the flat collection would return its source.
    QVERIFY(store.saveCatalogCache(makeCacheSnapshot()));

    const QString directory = m_settings.filePath(QStringLiteral("catalog-collection-cache"));
    QVERIFY(QDir().mkpath(directory));

    const QString firstPayloadPath = QDir(directory).filePath(QStringLiteral("flat-stars.txt"));
    const QString secondPayloadPath = QDir(directory).filePath(QStringLiteral("flat-deep-sky.txt"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            firstPayloadPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 909101, .hip = 909101, .properName = "Flat Star", .mag = "1.0"}
            )
        )
    );
    QVERIFY(CatalogTestPayloadFile::write(secondPayloadPath, skygate::ui::tests::sampleCompactOpenNgcCsvPayload()));

    // A flat collection written before generations were published: its
    // identities, order, and options are the committed configuration.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-stars"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:stars"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_stars"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat Stars"));
        settings.setValue(QStringLiteral("version"), QStringLiteral("v1"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-stars.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
        );
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 0);
        settings.setValue(QStringLiteral("payloadPath"), firstPayloadPath);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-dso"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:dso"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_dso"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat DSO"));
        settings.setValue(QStringLiteral("version"), QStringLiteral("v1"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-dso.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly)
        );
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 1);
        settings.setValue(QStringLiteral("payloadPath"), secondPayloadPath);
        settings.endGroup();
        settings.sync();
    }

    // An interrupted first upgrade staged a generation record without
    // publishing a manifest.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
        settings.beginGroup(QStringLiteral("catalogSources/4/catalog-source-staged"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("staged:source"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Source"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.endGroup();
        settings.sync();
    }

    // The flat records are the committed configuration, so the manager
    // restores them instead of the readable two-slot cache and upgrades them
    // to the current format.
    {
        SkyCatalogManager manager(&store);
        QVERIFY(manager.restoreCatalogCache());
        QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("flat:stars"), QStringLiteral("flat:dso")}));
        QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Flat Star")));
        QVERIFY(!manager.sourceTitles().values().contains(QStringLiteral("Custom")));
    }

    // The upgrade committed a generation with the same identities, options,
    // and order; the staged record is gone rather than promoted.
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.isLoaded());
    QCOMPARE(
        upgraded.snapshot.schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    QCOMPARE(upgraded.snapshot.sources.size(), 2);
    QCOMPARE(upgraded.snapshot.sources[0].instanceId, QStringLiteral("flat:stars"));
    QCOMPARE(upgraded.snapshot.sources[0].descriptorId, QStringLiteral("flat_stars"));
    QCOMPARE(upgraded.snapshot.sources[0].urls, QStringList{QStringLiteral("https://example.test/flat-stars.csv")});
    QCOMPARE(upgraded.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(upgraded.snapshot.sources[0].enabled);
    QCOMPARE(upgraded.snapshot.sources[0].order, 0);
    QCOMPARE(upgraded.snapshot.sources[1].instanceId, QStringLiteral("flat:dso"));
    QCOMPARE(upgraded.snapshot.sources[1].urls, QStringList{QStringLiteral("https://example.test/flat-dso.csv")});
    QCOMPARE(upgraded.snapshot.sources[1].policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QCOMPARE(upgraded.snapshot.sources[1].order, 1);

    // Two restarts restore the upgraded generation with the same identities
    // and order; the retired two-slot source never returns.
    for (int restart = 0; restart < 2; ++restart) {
        SkyCatalogManager restarted(&store);
        QVERIFY(restarted.restoreCatalogCache());
        QCOMPARE(
            restarted.sourceInstanceIds(), QStringList({QStringLiteral("flat:stars"), QStringLiteral("flat:dso")})
        );
        QVERIFY(CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Flat Star")));
        QVERIFY(!restarted.sourceTitles().values().contains(QStringLiteral("Custom")));
    }
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerPersistenceTests::unreadableCommittedCollectionKeepsActiveCollectionAndReportsFailure()
{
    // The retired pre-generation two-slot cache stays readable on disk, so a
    // restore that fell back to it would visibly resurrect its source.
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Custom");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908001, .hip = 908001, .properName = "Legacy Star", .mag = "1.0"}
    );

    SkySettingsStore::CatalogCollectionCacheSnapshot committed;
    committed.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    committed.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    SkySettingsStore::CatalogSourceCacheRecord accepted;
    accepted.instanceId = QStringLiteral("custom:accepted");
    accepted.title = QStringLiteral("Accepted Source");
    accepted.urls = QStringList{QStringLiteral("https://example.test/accepted-stars.csv")};
    accepted.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    accepted.enabled = true;
    accepted.order = 0;
    accepted.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908101, .hip = 908101, .properName = "Accepted Star", .mag = "1.0"}
    );
    committed.sources.push_back(std::move(accepted));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));
    QVERIFY(store.saveCatalogCollectionCache(committed));

    SkyCatalogManager manager(&store);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("custom:accepted")});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));

    // The committed manifest is lost while its records and the commit record
    // survive, so the readable legacy cache must not be migrated over the
    // committed configuration.
    const QString collectionCacheDirectory = m_settings.filePath(QStringLiteral("catalog-collection-cache"));
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(collectionCacheDirectory)));

    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    const std::size_t bodyCountBefore = manager.bodyCount();
    const std::uint64_t revisionBefore = manager.catalogRevision();

    QVERIFY(!manager.restoreCatalogCache());

    // The accepted collection stays active: no legacy source returns and no
    // catalog change is published for the rejected restore.
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("custom:accepted")});
    QCOMPARE(manager.bodyCount(), bodyCountBefore);
    QCOMPARE(manager.catalogRevision(), revisionBefore);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
    QCOMPARE(catalogSpy.count(), 0);

    // The failure is reported once through the normal status signal, naming
    // the unreadable configuration and the recovery action.
    QCOMPARE(statusSpy.count(), 1);
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Saved catalog configuration unreadable")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Add, update, or reorder a catalog source")));
    QVERIFY(manager.statusText().contains(QStringLiteral("recorded as committed")));

    // A later start reaches the same decision: the rejected restore left the
    // stored state and the readable legacy cache untouched, and neither of
    // them replaces the first-use default.
    SkyCatalogManager restarted(&store);
    QVERIFY(!restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
    QVERIFY(store.loadCatalogCache().has_value());
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

void SkyCatalogManagerPersistenceTests::liveRestoreDropsOperationsOmittedFromTheRestoredCollection()
{
    const QString catalogUrl = QStringLiteral("https://example.test/live-restore-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910200, .hip = 910200, .properName = "Live Restore Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.sourceViewEntries().size(), 2);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Live Restore Star")));

    // The committed configuration is now empty even though this manager still
    // holds the source it loaded, as after a settings restore that replaces
    // the session's collection.
    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    QVERIFY(manager.restoreCatalogCache());

    // The accepted restore replaces the runtime collection and the operation
    // rows together: an omitted source stays neither active nor visible as a
    // leftover pending row.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceInstanceIds().isEmpty());
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Live Restore Star")));

    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerPersistenceTests::liveRestoreSupersedesPendingImportReply()
{
    const QString pendingUrl = QStringLiteral("https://example.test/live-restore-pending-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        pendingUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910001, .hip = 910001, .properName = "Superseded Pending Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(pendingUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QVERIFY(manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, pendingUrl);
    QVERIFY(!pendingReply.isNull());

    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    QVERIFY(manager.restoreCatalogCache());
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceViewEntries().isEmpty());
    const std::uint64_t revisionAfterRestore = manager.catalogRevision();

    pendingReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The reply of the superseded import cannot repopulate the accepted
    // collection, create a row, or publish a catalog change.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QCOMPARE(manager.catalogRevision(), revisionAfterRestore);
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Superseded Pending Star")));

    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerPersistenceTests::liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId()
{
    const QString catalogUrl = QStringLiteral("https://example.test/live-restore-reused-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/live-restore-reused-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    // The stored configuration names the same instance the live manager is
    // already waiting on a related reply for, so the restored source reuses
    // the pending instance's ID.
    const SkySettingsStore::CatalogCollectionCacheLoadResult beforeRestore = store.loadCatalogCollectionCache();
    QVERIFY(beforeRestore.isLoaded());
    QVERIFY(CatalogSourceFactsQuery::containsInstanceId(beforeRestore.snapshot, instanceId));

    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    const std::uint64_t revisionAfterRestore = manager.catalogRevision();
    const QString statusAfterRestore = manager.statusText();

    relatedReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The previous incarnation's pending related reply cannot attach its
    // dataset to the restored instance that reused the ID, and it leaves the
    // restored counts, status, and persisted record untouched.
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterRestore);
    QCOMPARE(manager.statusText(), statusAfterRestore);

    const SkySettingsStore::CatalogCollectionCacheLoadResult afterStale = store.loadCatalogCollectionCache();
    QVERIFY(afterStale.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(afterStale.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(record->constellationCount, std::size_t{0});
}

void SkyCatalogManagerPersistenceTests::rejectedRestoreKeepsPendingImportWork()
{
    const QString pendingUrl = QStringLiteral("https://example.test/rejected-restore-pending-stars.csv");

    // A persisted instance identity colliding with the implicit bundled core
    // source makes the restored collection uncomposable, so the restore is
    // rejected before it can replace the live collection.
    SkySettingsStore::CatalogCollectionCacheSnapshot rejectedSnapshot;
    rejectedSnapshot.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    SkySettingsStore::CatalogSourceCacheRecord colliding;
    colliding.instanceId = QStringLiteral("bundled-core");
    colliding.title = QStringLiteral("Colliding Source");
    colliding.urls = QStringList{QStringLiteral("https://example.test/colliding-stars.csv")};
    colliding.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    colliding.enabled = true;
    colliding.order = 0;
    colliding.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Colliding Star", .mag = "1.0"}
    );
    rejectedSnapshot.sources.push_back(std::move(colliding));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(rejectedSnapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        pendingUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910101, .hip = 910101, .properName = "Still Valid Pending Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);
    const skygate::ui::internal::SkyCatalogSourceInstance pending =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(pendingUrl);
    const QString instanceId = pending.instanceId;
    manager.loadSource(pending, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, pendingUrl);
    QVERIFY(!pendingReply.isNull());

    const QStringList sourcesBefore = manager.sourceInstanceIds();
    const QVector<SkyCatalogManager::SourceViewEntry> rowsBefore = manager.sourceViewEntries();
    QCOMPARE(rowsBefore.size(), 2);
    const int statusChangesAfterStart = statusSpy.count();

    QVERIFY(!manager.restoreCatalogCache());
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Collection rejected")));
    QCOMPARE(statusSpy.count(), statusChangesAfterStart + 1);

    // The rejected restore keeps the previous collection, its rows, and the
    // still-valid import instead of discarding the pending work.
    QCOMPARE(manager.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(manager.sourceViewEntries().size(), rowsBefore.size());
    QVERIFY(manager.downloadingCatalog());
    const QVector<SkyCatalogManager::SourceViewEntry> rowsAfterRejection = manager.sourceViewEntries();
    const auto pendingRow =
        std::find_if(rowsAfterRejection.begin(), rowsAfterRejection.end(), [&instanceId](const auto& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(pendingRow != rowsAfterRejection.end());
    QVERIFY(pendingRow->busy);

    pendingReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The preserved import still completes into the unchanged collection.
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.sourceInstanceIds().contains(instanceId));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Still Valid Pending Star")));
}

QTEST_GUILESS_MAIN(SkyCatalogManagerPersistenceTests)

#include "SkyCatalogManagerPersistenceTests.moc"
