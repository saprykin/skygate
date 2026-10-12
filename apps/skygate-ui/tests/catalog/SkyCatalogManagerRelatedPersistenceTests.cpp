#include "CatalogSourceFactsQuery.hpp"
#include "ConstellationAnchorGroupLookup.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "FakeReplyQuery.hpp"
#include "RelatedDatasetFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

class SkyCatalogManagerRelatedPersistenceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void clearSourceCacheEvictsOwnedRelatedPayload();
    void relatedReplyAfterCacheEvictionStaysUnpersisted();
    void relatedDatasetsRoundTripToTheirOwnSources();
    void disabledOwnerDataStaysOwnedButInactive();
    void removedOwnerRelatedDataDoesNotReturnAfterRestart();
    void corruptOwnerPayloadLeavesSiblingDatasetIntact();
    void migratesPriorSingleOwnerRelatedPayloadOnce();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerRelatedPersistenceTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerRelatedPersistenceTests")));
}

void SkyCatalogManagerRelatedPersistenceTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerRelatedPersistenceTests::clearSourceCacheEvictsOwnedRelatedPayload()
{
    const QString sourceUrl = QStringLiteral("https://example.test/clear-related-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/clear-related-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.payload = skygate::ui::tests::stellariumConstellationIndexJsonPayload(
             {{QStringLiteral("orion"), {27989, 25336, 25930}}}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceUrl);
    source.relatedDatasetUrls = QStringList{relatedUrl};
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const auto withRelated = store.loadCatalogCollectionCache();
    QVERIFY(withRelated.isLoaded());
    const SkyCatalogSourceCacheRecord* recordWithRelated =
        CatalogSourceFactsQuery::findRecord(withRelated.snapshot, sourceId);
    QVERIFY(recordWithRelated != nullptr);
    QVERIFY(!recordWithRelated->payload.isEmpty());
    QVERIFY(!recordWithRelated->constellationLineRows.isEmpty());
    QVERIFY(!recordWithRelated->constellationAnchorGroupRows.isEmpty());

    QVERIFY(manager.clearSourceCache(sourceId));

    // The accepted snapshot keeps contributing until a reload or restart, but
    // the owned related dataset is payload of the same source and is evicted
    // together with its catalog payload.
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    const auto evicted = store.loadCatalogCollectionCache();
    QVERIFY(evicted.isLoaded());
    const SkyCatalogSourceCacheRecord* evictedRecord = CatalogSourceFactsQuery::findRecord(evicted.snapshot, sourceId);
    QVERIFY(evictedRecord != nullptr);
    QVERIFY(evictedRecord->payload.isEmpty());
    QVERIFY(evictedRecord->binaryPayload.isEmpty());
    QVERIFY(evictedRecord->constellationLineRows.isEmpty());
    QVERIFY(evictedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(evictedRecord->constellationCount, std::size_t{0});
    QCOMPARE(evictedRecord->relatedDatasetUrls, QStringList{relatedUrl});

    // A metadata-only persistence of the collection must not write the still
    // retained in-memory related dataset back into the evicted record.
    manager.disableSource(sourceId);
    manager.enableSource(sourceId);
    const auto afterToggle = store.loadCatalogCollectionCache();
    QVERIFY(afterToggle.isLoaded());
    const SkyCatalogSourceCacheRecord* toggledRecord =
        CatalogSourceFactsQuery::findRecord(afterToggle.snapshot, sourceId);
    QVERIFY(toggledRecord != nullptr);
    QVERIFY(toggledRecord->enabled);
    QVERIFY(toggledRecord->payload.isEmpty());
    QVERIFY(toggledRecord->constellationLineRows.isEmpty());
    QVERIFY(toggledRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(toggledRecord->constellationCount, std::size_t{0});
}

void SkyCatalogManagerRelatedPersistenceTests::relatedReplyAfterCacheEvictionStaysUnpersisted()
{
    const QString catalogUrl = QStringLiteral("https://example.test/evict-related-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/evict-related-lines.json");

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

    // The main load is accepted while its related reply is still pending.
    const auto beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkyCatalogSourceCacheRecord* recordBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, instanceId);
    QVERIFY(recordBeforeClear != nullptr);
    QVERIFY(!recordBeforeClear->payload.isEmpty());

    QVERIFY(manager.clearSourceCache(instanceId));
    skygate::ui::tests::FakeNetworkReply* pendingReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(pendingReply != nullptr);
    QVERIFY(!pendingReply->isFinished());

    // The related download completes after the eviction. The runtime keeps the
    // dataset for the session, but persistence must not write it into the
    // evicted record.
    pendingReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const auto afterEviction = store.loadCatalogCollectionCache();
    QVERIFY(afterEviction.isLoaded());
    const SkyCatalogSourceCacheRecord* evictedRecord =
        CatalogSourceFactsQuery::findRecord(afterEviction.snapshot, instanceId);
    QVERIFY(evictedRecord != nullptr);
    QVERIFY(evictedRecord->payload.isEmpty());
    QVERIFY(evictedRecord->constellationLineRows.isEmpty());
    QVERIFY(evictedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(evictedRecord->constellationCount, std::size_t{0});

    // Restart: the source stays configured and reports the unavailable payload
    // state; the related data that landed after the eviction did not sneak
    // back into the record.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Saved catalog source cache has no payload")));
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    const QVector<SkyCatalogManager::SourceViewEntry> view = restarted.sourceViewEntries();
    QCOMPARE(view.size(), std::size_t{2});
    const auto evictedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(evictedEntry != view.end());
    QVERIFY(evictedEntry->hasError);
    QVERIFY(!evictedEntry->busy);
    QCOMPARE(evictedEntry->statusText, QStringLiteral("Payload unavailable"));
}

void SkyCatalogManagerRelatedPersistenceTests::relatedDatasetsRoundTripToTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/roundtrip-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/roundtrip-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/roundtrip-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/roundtrip-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* recordA = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceAId);
    const SkyCatalogSourceCacheRecord* recordB = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);

    // Each record carries the dataset its own owner downloaded; the composed
    // collection-wide view is not copied into both records.
    const auto recordALines = CatalogSourceFactsQuery::relatedLineRefs(*recordA);
    const auto recordBLines = CatalogSourceFactsQuery::relatedLineRefs(*recordB);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QCOMPARE(recordBLines.size(), std::size_t{2});
    QVERIFY(CatalogSourceFactsQuery::relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!CatalogSourceFactsQuery::relatedLineRefsContainHip(recordALines, "hip_26311"));
    QVERIFY(CatalogSourceFactsQuery::relatedLineRefsContainHip(recordBLines, "hip_26311"));
    QVERIFY(!CatalogSourceFactsQuery::relatedLineRefsContainHip(recordBLines, "hip_27989"));

    // The restart restores each dataset to its own owner.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(restoredManager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});

    restoredManager.disableSource(sourceAId);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Orion") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    restoredManager.enableSource(sourceAId);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
}

void SkyCatalogManagerRelatedPersistenceTests::disabledOwnerDataStaysOwnedButInactive()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/disabled-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/disabled-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/disabled-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/disabled-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* recordA = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceAId);
    const SkyCatalogSourceCacheRecord* recordB = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);
    QVERIFY(!recordA->enabled);

    // The disabled owner keeps its own inactive dataset; the persist does not
    // replace it with the composed view of the enabled sibling.
    const auto recordALines = CatalogSourceFactsQuery::relatedLineRefs(*recordA);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QVERIFY(CatalogSourceFactsQuery::relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!CatalogSourceFactsQuery::relatedLineRefsContainHip(recordALines, "hip_26311"));
    QCOMPARE(recordA->constellationCount, std::size_t{1});

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(!restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));

    // The disabled owner's references stay inactive after the restart.
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    // Re-enabling restores the owner's own dataset, not the sibling's.
    restoredManager.enableSource(sourceAId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    const skygate::ephemeris::ConstellationAnchorGroup* orionGroup =
        ConstellationAnchorGroupLookup::find(restoredManager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.size(), std::size_t{3});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});
}

void SkyCatalogManagerRelatedPersistenceTests::removedOwnerRelatedDataDoesNotReturnAfterRestart()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // A first restart restores both owners from their own records.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);

    // Removing one owner removes its dataset with its record.
    restoredManager.removeSource(sourceBId);
    QVERIFY(!restoredManager.sourceInstanceIds().contains(sourceBId));
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    const auto afterRemoval = store.loadCatalogCollectionCache();
    QVERIFY(afterRemoval.isLoaded());
    QVERIFY(!CatalogSourceFactsQuery::containsInstanceId(afterRemoval.snapshot, sourceBId));
    const SkyCatalogSourceCacheRecord* survivingRecord =
        CatalogSourceFactsQuery::findRecord(afterRemoval.snapshot, sourceAId);
    QVERIFY(survivingRecord != nullptr);
    const auto survivingLines = CatalogSourceFactsQuery::relatedLineRefs(*survivingRecord);
    QVERIFY(!CatalogSourceFactsQuery::relatedLineRefsContainHip(survivingLines, "hip_26311"));

    // The retired pre-collection two-slot cache still holds related data on
    // disk, but a committed collection snapshot keeps that fallback unused.
    SkyCatalogLegacyCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Related");
    legacy.catalogPayload = skygate::ui::tests::orionHygCsvPayload();
    legacy.constellationLineRows = "hip_26311|hip_26727\n";
    legacy.constellationAnchorGroupRows = "Lyra|hip_26311,hip_26727\n";
    legacy.constellationLineSchemaVersion =
        skygate::ui::internal::SkyCatalogCollectionStore::kConstellationLineCacheSchemaVersion;
    legacy.constellationCount = 1U;
    QVERIFY(store.saveCatalogCache(legacy));

    // The removed owner's dataset does not return through another record or
    // through the legacy fallback.
    SkyCatalogManager secondRestart(&store);
    QVERIFY(secondRestart.restoreCatalogCache());
    QVERIFY(!secondRestart.sourceInstanceIds().contains(sourceBId));
    QVERIFY(secondRestart.sourceInstanceIds().contains(sourceAId));
    QCOMPARE(secondRestart.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(secondRestart, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(secondRestart, "Lyra") == nullptr);
    QCOMPARE(secondRestart.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerRelatedPersistenceTests::corruptOwnerPayloadLeavesSiblingDatasetIntact()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/corrupt-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/corrupt-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/corrupt-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/corrupt-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // Corrupt one owner's stored related payload in place.
    auto corruptedSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(corruptedSnapshot.isLoaded());
    const auto corruptedRecord = std::find_if(
        corruptedSnapshot.snapshot.sources.begin(),
        corruptedSnapshot.snapshot.sources.end(),
        [&sourceAId](const SkyCatalogSourceCacheRecord& record) { return record.instanceId == sourceAId; }
    );
    QVERIFY(corruptedRecord != corruptedSnapshot.snapshot.sources.end());
    corruptedRecord->constellationLineRows = "not a related line payload";
    QVERIFY(store.saveCatalogCollectionCache(corruptedSnapshot.snapshot));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Saved related constellation dataset is unreadable"));
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());

    // The corrupted owner restores without related data and keeps its
    // configuration; the sibling's dataset is untouched.
    QVERIFY(restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    restoredManager.disableSource(sourceBId);
    QVERIFY(restoredManager.constellationLineRefs().empty());
    restoredManager.enableSource(sourceBId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerRelatedPersistenceTests::migratesPriorSingleOwnerRelatedPayloadOnce()
{
    // A snapshot written before the per-source related format: the bundled
    // record and one star record, where the star record holds the only copy of
    // the then-collection-wide related dataset.
    SkyCatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion - 1;

    SkyCatalogSourceCacheRecord bundled;
    bundled.instanceId = QStringLiteral("primary");
    bundled.title = QStringLiteral("Bundled");
    bundled.bundled = true;
    bundled.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bundled.enabled = true;
    bundled.order = 0;
    snapshot.sources.push_back(std::move(bundled));

    SkyCatalogSourceCacheRecord star;
    star.instanceId = QStringLiteral("custom:prior-owner");
    star.title = QStringLiteral("Prior Owner");
    star.urls = QStringList{QStringLiteral("https://example.test/prior-owner.csv")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/prior-owner-lines.json")};
    star.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.order = 1;
    star.payload = skygate::ui::tests::orionHygCsvPayload();
    star.constellationLineRows = "hip_27989|hip_25336\nhip_25336|hip_25930\n";
    star.constellationAnchorGroupRows = "Orion|hip_27989,hip_25336\n";
    star.constellationLineSchemaVersion =
        skygate::ui::internal::SkyCatalogCollectionStore::kConstellationLineCacheSchemaVersion;
    star.constellationCount = 1U;
    snapshot.sources.push_back(std::move(star));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    // The sole related payload identifies its owner, so the migration adopts
    // it instead of discarding it.
    SkyCatalogManager manager(&store);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    // The migration rewrite commits the record in the per-source format and
    // keeps the adopted dataset under its owner.
    const auto migrated = store.loadCatalogCollectionCache();
    QVERIFY(migrated.isLoaded());
    QCOMPARE(
        migrated.snapshot.schemaVersion, skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion
    );
    const SkyCatalogSourceCacheRecord* migratedRecord =
        CatalogSourceFactsQuery::findRecord(migrated.snapshot, QStringLiteral("custom:prior-owner"));
    QVERIFY(migratedRecord != nullptr);
    QCOMPARE(CatalogSourceFactsQuery::relatedLineRefs(*migratedRecord).size(), std::size_t{2});

    // The next start reads the migrated records without another migration and
    // with the owner's dataset intact.
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(restarted, "Orion") != nullptr);
    QCOMPARE(restarted.constellationCount(), std::size_t{1});
}

QTEST_GUILESS_MAIN(SkyCatalogManagerRelatedPersistenceTests)

#include "SkyCatalogManagerRelatedPersistenceTests.moc"
