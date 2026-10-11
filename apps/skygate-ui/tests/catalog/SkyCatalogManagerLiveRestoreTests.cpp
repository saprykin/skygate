#include "CatalogBodyQuery.hpp"
#include "CatalogSourceFactsQuery.hpp"
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
#include <QPointer>
#include <QSignalSpy>
#include <QThreadPool>
#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

class SkyCatalogManagerLiveRestoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void liveRestoreDropsOperationsOmittedFromTheRestoredCollection();
    void liveRestoreSupersedesPendingImportReply();
    void liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId();
    void rejectedRestoreKeepsPendingImportWork();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerLiveRestoreTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerLiveRestoreTests")));
}

void SkyCatalogManagerLiveRestoreTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerLiveRestoreTests::liveRestoreDropsOperationsOmittedFromTheRestoredCollection()
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

void SkyCatalogManagerLiveRestoreTests::liveRestoreSupersedesPendingImportReply()
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

void SkyCatalogManagerLiveRestoreTests::liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId()
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

void SkyCatalogManagerLiveRestoreTests::rejectedRestoreKeepsPendingImportWork()
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

QTEST_GUILESS_MAIN(SkyCatalogManagerLiveRestoreTests)

#include "SkyCatalogManagerLiveRestoreTests.moc"
