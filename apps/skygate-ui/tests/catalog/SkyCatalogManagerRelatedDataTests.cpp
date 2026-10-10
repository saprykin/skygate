#include "CatalogBodyQuery.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogNotificationRecorder.hpp"
#include "CatalogSourceFactsQuery.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationAnchorGroupLookup.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "FakeReplyQuery.hpp"
#include "RelatedDatasetFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogNotificationRecorder;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

namespace {

constexpr int kStaleConstellationDelayMs = 500;

const skygate::ui::internal::SkyCatalogSourceDescriptor kHygPreset =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

bool allIssuedRepliesFinished(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager)
{
    const auto replies = networkAccessManager.issuedReplies();
    return std::all_of(replies.begin(), replies.end(), [](const skygate::ui::tests::FakeNetworkReply* reply) {
        return reply == nullptr || reply->isFinished();
    });
}

}  // namespace

class SkyCatalogManagerRelatedDataTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void staleConstellationResponseIgnoredAfterBundledSwitch();
    void staleConstellationResponseIgnoredAfterCustomSwitch();
    void cancelDuringConstellationLoadingIgnoresStaleCompletion();
    void currentConstellationResponseAppliesOnce();
    void clearSourceCacheEvictsOwnedRelatedPayload();
    void relatedReplyAfterCacheEvictionStaysOutOfPersistedRecord();
    void failedUpdateSupersedesItsPendingRelatedResponse();
    void relatedConstellationDatasetsStayOwnedByTheirSources();
    void overlappingConstellationDatasetsFollowSourceOrder();
    void lateRelatedResponseAfterOwnerRemovalIsIgnored();
    void readdedOwnerRejectsPreviousIncarnationResponse();
    void disablingOwnerSupersedesItsPendingRelatedResponse();
    void lateRelatedFailureStatusFromSupersededOwnerIsIgnored();
    void outOfOrderRelatedRepliesPopulateTheirOwnSources();
    void reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse();
    void reloadClearsOwnerRelatedDataBeforePublishing();
    void failedRelatedReplacementKeepsPublishedStateSynchronized();
    void canceledRelatedReplacementKeepsPublishedStateSynchronized();
    void reloadClearsOnlyOwningRelatedDataset();
    void relatedDatasetsRoundTripToTheirOwnSources();
    void disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart();
    void removedOwnerRelatedDataDoesNotReturnAfterRestart();
    void corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact();
    void migratesPriorSingleOwnerRelatedPayloadOnce();
    void updateRemovingRelatedDeclarationRetiresOwnedData();
    void failedUpdateAttemptingToRemoveRelatedDeclarationKeepsDataset();
    void lateReplyToRemovedRelatedDeclarationIsRejected();
    void bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy_data();
    void bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy();
    void payloadOriginSwitchesFollowTheAcceptedRelatedDeclaration();
    void rejectedBundledUpdateKeepsOwnedRelatedData();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerRelatedDataTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerRelatedDataTests")));
}

void SkyCatalogManagerRelatedDataTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerRelatedDataTests::staleConstellationResponseIgnoredAfterBundledSwitch()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900010, .properName = "Pending HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> constellationReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, constellationUrl);
    QVERIFY(!constellationReply.isNull());
    QVERIFY(!constellationReply->isFinished());

    manager.loadCatalogPreset(QStringLiteral("bundled"));
    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    const auto revisionAfterSwitch = manager.catalogRevision();
    const int catalogChangesAfterSwitch = catalogSpy.count();
    const QString statusAfterSwitch = manager.statusText();
    const int statusChangesAfterSwitch = statusSpy.count();

    if (!constellationReply.isNull()) {
        skygate::ui::tests::waitForFakeReplyFinished(constellationReply.data());
    }
    QCoreApplication::processEvents();

    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterSwitch);
    QCOMPARE(manager.statusText(), statusAfterSwitch);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterSwitch);
    QCOMPARE(statusSpy.count(), statusChangesAfterSwitch);
}

void SkyCatalogManagerRelatedDataTests::staleConstellationResponseIgnoredAfterCustomSwitch()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    const QString customUrl = QStringLiteral("https://example.test/custom-stars.csv");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900011, .properName = "Pending HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );
    networkAccessManager.enqueueResponse(
        customUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 900012, .properName = "Custom Star", .mag = "2.0"})}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> constellationReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, constellationUrl);
    QVERIFY(!constellationReply.isNull());
    QVERIFY(!constellationReply->isFinished());

    manager.downloadCatalogFromUrl(customUrl);
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("Downloaded"));
    QVERIFY(manager.constellationLineRefs().empty());
    const auto revisionAfterSwitch = manager.catalogRevision();
    const int catalogChangesAfterSwitch = catalogSpy.count();
    const QString statusAfterSwitch = manager.statusText();
    const int statusChangesAfterSwitch = statusSpy.count();

    if (!constellationReply.isNull()) {
        skygate::ui::tests::waitForFakeReplyFinished(constellationReply.data());
    }
    QCoreApplication::processEvents();

    QCOMPARE(manager.sourceLabel(), QStringLiteral("Downloaded"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterSwitch);
    QCOMPARE(manager.statusText(), statusAfterSwitch);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterSwitch);
    QCOMPARE(statusSpy.count(), statusChangesAfterSwitch);

    const auto cacheSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(cacheSnapshot.isLoaded());
    QCOMPARE(cacheSnapshot.snapshot.sources.size(), 1);
    QVERIFY(cacheSnapshot.snapshot.sources[0].constellationLineRows.isEmpty());
    QVERIFY(cacheSnapshot.snapshot.sources[0].constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerRelatedDataTests::cancelDuringConstellationLoadingIgnoresStaleCompletion()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.hip = 900013, .properName = "Cancel Pending HYG Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(!manager.downloadingCatalog());

    const auto revisionBeforeCancel = manager.catalogRevision();
    const int catalogChangesBeforeCancel = catalogSpy.count();

    manager.cancelCatalogDownload();
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(manager.constellationLineRefs().empty());

    QTRY_VERIFY(networkAccessManager.requestedUrls().size() >= 4);
    QTRY_VERIFY(allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();

    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), catalogChangesBeforeCancel);
}

void SkyCatalogManagerRelatedDataTests::currentConstellationResponseAppliesOnce()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900014, .properName = "Current HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl, {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(manager.constellationLineRefs().size() == 2U);

    QCOMPARE(manager.constellationLineRefs().size(), 2U);
    QCOMPARE(manager.constellationAnchorGroups().size(), 1U);
    QCOMPARE(manager.constellationCount(), 1U);

    const auto revisionAfterApply = manager.catalogRevision();
    const int catalogChangesAfterApply = catalogSpy.count();

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(manager.constellationLineRefs().size(), 2U);
    QCOMPARE(manager.constellationAnchorGroups().size(), 1U);
    QCOMPARE(manager.catalogRevision(), revisionAfterApply);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterApply);
}

void SkyCatalogManagerRelatedDataTests::clearSourceCacheEvictsOwnedRelatedPayload()
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
    const SkySettingsStore::CatalogSourceCacheRecord* recordWithRelated =
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
    const SkySettingsStore::CatalogSourceCacheRecord* evictedRecord =
        CatalogSourceFactsQuery::findRecord(evicted.snapshot, sourceId);
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
    const SkySettingsStore::CatalogSourceCacheRecord* toggledRecord =
        CatalogSourceFactsQuery::findRecord(afterToggle.snapshot, sourceId);
    QVERIFY(toggledRecord != nullptr);
    QVERIFY(toggledRecord->enabled);
    QVERIFY(toggledRecord->payload.isEmpty());
    QVERIFY(toggledRecord->constellationLineRows.isEmpty());
    QVERIFY(toggledRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(toggledRecord->constellationCount, std::size_t{0});
}

void SkyCatalogManagerRelatedDataTests::relatedReplyAfterCacheEvictionStaysOutOfPersistedRecord()
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
    const SkySettingsStore::CatalogSourceCacheRecord* recordBeforeClear =
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
    const SkySettingsStore::CatalogSourceCacheRecord* evictedRecord =
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

void SkyCatalogManagerRelatedDataTests::failedUpdateSupersedesItsPendingRelatedResponse()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/superseded-accepted.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/superseded-accepted-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/superseded-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        RelatedDatasetFixture::relatedDatasetInstance(acceptedUrl, relatedUrl);
    accepted.title = QStringLiteral("Superseded Accepted Title");
    accepted.version = QStringLiteral("v1");
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    // The update attempt fails, which supersedes the pending related reply of
    // the previous incarnation while the accepted catalog stays active.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Superseded Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/superseded-rejected-lines.json")};

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/superseded-update\\.csv .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QString statusAfterFailure = manager.statusText();
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The superseded reply cannot attach its data to the still-accepted source
    // or overwrite its operation error.
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.statusText(), statusAfterFailure);

    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Superseded Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(record->constellationLineRows.isEmpty());
}

void SkyCatalogManagerRelatedDataTests::relatedConstellationDatasetsStayOwnedByTheirSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/related-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/related-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/related-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/related-b-lines.json");

    const QByteArray sourceARelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("orion"), {27989, 25336, 25930}}});
    const QByteArray sourceBRelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("lyra"), {26311, 26727, 24436}}});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = sourceARelatedPayload});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = sourceBRelatedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const auto loadRelatedSource = [&](const skygate::ui::internal::SkyCatalogSourceInstance& instance) {
        manager.loadSource(instance, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceAUrl);
    sourceA.relatedDatasetUrls = QStringList{sourceARelatedUrl};
    const QString sourceAId = sourceA.instanceId;
    loadRelatedSource(sourceA);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceBUrl);
    sourceB.relatedDatasetUrls = QStringList{sourceBRelatedUrl};
    const QString sourceBId = sourceB.instanceId;
    loadRelatedSource(sourceB);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // Two loaded sources retain distinct related datasets simultaneously.
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    // Disabling one source removes only its owned dataset from the active
    // view, including its anchors and its count contribution.
    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    // Re-enabling restores the retained dataset.
    manager.enableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Removing one source does not remove the other source's dataset, and the
    // surviving references still resolve against the active object identities.
    manager.removeSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QCOMPARE(manager.resolvedConstellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.resolvedConstellationAnchorGroups().size(), std::size_t{1});
    QVERIFY(manager.isSourceEnabled(sourceBId));
}

void SkyCatalogManagerRelatedDataTests::overlappingConstellationDatasetsFollowSourceOrder()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/overlap-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/overlap-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/overlap-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/overlap-b-lines.json");

    // Source A declares three constellations but only two extract anchor
    // groups, so the declared count and the composed group count differ.
    const QByteArray sourceARelatedPayload = skygate::ui::tests::stellariumConstellationIndexJsonPayload(
        {{QStringLiteral("orion"), {27989, 25336, 25930}},
         {QStringLiteral("cepheus"), {26311, 26727, 24436}},
         {QStringLiteral("draco"), {42}}}
    );
    const QByteArray sourceBRelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("orion"), {24436, 25930, 26727}}});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = sourceARelatedPayload});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = sourceBRelatedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const auto loadRelatedSource = [&](const skygate::ui::internal::SkyCatalogSourceInstance& instance) {
        manager.loadSource(instance, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceAUrl);
    sourceA.relatedDatasetUrls = QStringList{sourceARelatedUrl};
    const QString sourceAId = sourceA.instanceId;
    loadRelatedSource(sourceA);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{3});

    skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceBUrl);
    sourceB.relatedDatasetUrls = QStringList{sourceBRelatedUrl};
    loadRelatedSource(sourceB);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{6});

    // The later enabled source owns the overlapping constellation name and the
    // declared count follows the last enabled owner that declares one.
    const auto* orionGroup = ConstellationAnchorGroupLookup::find(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_24436"));
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Reordering the collection changes both the winning anchors and the
    // composed declared count.
    manager.moveSource(sourceAId, 2);
    orionGroup = ConstellationAnchorGroupLookup::find(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_25336"));
    QCOMPARE(manager.constellationCount(), std::size_t{3});
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{6});
}

void SkyCatalogManagerRelatedDataTests::lateRelatedResponseAfterOwnerRemovalIsIgnored()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);

    manager.removeSource(sourceAId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceAId));
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    const auto revisionAfterRemoval = manager.catalogRevision();
    const int catalogChangesAfterRemoval = catalogSpy.count();
    const QString statusAfterRemoval = manager.statusText();
    const int statusChangesAfterRemoval = statusSpy.count();

    const auto entriesAfterRemoval = manager.sourceViewEntries();
    const auto siblingAfterRemoval = std::find_if(
        entriesAfterRemoval.begin(),
        entriesAfterRemoval.end(),
        [&sourceBId](const SkyCatalogManager::SourceViewEntry& entry) { return entry.instanceId == sourceBId; }
    );
    QVERIFY(siblingAfterRemoval != entriesAfterRemoval.end());
    QVERIFY(!siblingAfterRemoval->busy);
    QVERIFY(!siblingAfterRemoval->hasError);
    QCOMPARE(siblingAfterRemoval->statusText, QStringLiteral("Active"));

    const auto persistedAfterRemoval = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterRemoval.isLoaded());
    QVERIFY(!CatalogSourceFactsQuery::containsInstanceId(persistedAfterRemoval.snapshot, sourceAId));

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The removed owner's delayed response is discarded: no active lines or
    // counts, no catalog change, no status change, and no persisted owner data.
    // The retained sibling keeps its own dataset and status.
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(manager.catalogRevision(), revisionAfterRemoval);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterRemoval);
    QCOMPARE(manager.statusText(), statusAfterRemoval);
    QCOMPARE(statusSpy.count(), statusChangesAfterRemoval);

    const auto entriesAfterReply = manager.sourceViewEntries();
    const auto siblingAfterReply = std::find_if(
        entriesAfterReply.begin(),
        entriesAfterReply.end(),
        [&sourceBId](const SkyCatalogManager::SourceViewEntry& entry) { return entry.instanceId == sourceBId; }
    );
    QVERIFY(siblingAfterReply != entriesAfterReply.end());
    QVERIFY(!siblingAfterReply->busy);
    QVERIFY(!siblingAfterReply->hasError);
    QCOMPARE(siblingAfterReply->statusText, QStringLiteral("Active"));

    const auto persistedAfterReply = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterReply.isLoaded());
    QCOMPARE(persistedAfterReply.snapshot.sources.size(), persistedAfterRemoval.snapshot.sources.size());
    QVERIFY(!CatalogSourceFactsQuery::containsInstanceId(persistedAfterReply.snapshot, sourceAId));
}

void SkyCatalogManagerRelatedDataTests::readdedOwnerRejectsPreviousIncarnationResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/readded-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/readded-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance first =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = first.instanceId;
    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> staleReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!staleReply.isNull());
    QVERIFY(!staleReply->isFinished());

    // Replace the owner with a new incarnation of the same durable instance
    // ID; only this incarnation's revision may accept the related response.
    manager.removeSource(sourceId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceId));

    skygate::ui::internal::SkyCatalogSourceInstance second =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    second.instanceId = sourceId;
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    const auto revisionAfterCurrent = manager.catalogRevision();
    const int catalogChangesAfterCurrent = catalogSpy.count();
    const QString statusAfterCurrent = manager.statusText();
    const int statusChangesAfterCurrent = statusSpy.count();

    const auto persistedAfterCurrent = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterCurrent.isLoaded());
    const auto currentRecord = std::find_if(
        persistedAfterCurrent.snapshot.sources.begin(),
        persistedAfterCurrent.snapshot.sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(currentRecord != persistedAfterCurrent.snapshot.sources.end());
    const QByteArray persistedLineRows = currentRecord->constellationLineRows;
    QVERIFY(!persistedLineRows.isEmpty());

    QVERIFY(!staleReply.isNull());
    staleReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The previous incarnation's delayed response cannot replace the current
    // owner's dataset, its counts, its status, or the persisted collection.
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(manager.catalogRevision(), revisionAfterCurrent);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterCurrent);
    QCOMPARE(manager.statusText(), statusAfterCurrent);
    QCOMPARE(statusSpy.count(), statusChangesAfterCurrent);

    const auto persistedAfterStale = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterStale.isLoaded());
    const auto staleRecord = std::find_if(
        persistedAfterStale.snapshot.sources.begin(),
        persistedAfterStale.snapshot.sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(staleRecord != persistedAfterStale.snapshot.sources.end());
    QCOMPARE(staleRecord->constellationLineRows, persistedLineRows);
}

void SkyCatalogManagerRelatedDataTests::disablingOwnerSupersedesItsPendingRelatedResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/disabled-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/disabled-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    manager.disableSource(sourceId);
    QVERIFY(!manager.isSourceEnabled(sourceId));

    const auto revisionAfterDisable = manager.catalogRevision();
    const int catalogChangesAfterDisable = catalogSpy.count();
    const int datasetChangesAfterDisable = datasetSpy.count();
    const QString statusAfterDisable = manager.statusText();
    const int statusChangesAfterDisable = statusSpy.count();

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Disabling supersedes the owner's in-flight related work: the response is
    // discarded and cannot activate references or change counts or status.
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterDisable);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterDisable);
    QCOMPARE(datasetSpy.count(), datasetChangesAfterDisable);
    QCOMPARE(manager.statusText(), statusAfterDisable);
    QCOMPARE(statusSpy.count(), statusChangesAfterDisable);

    // Re-enabling restores only data that completed before the disable.
    manager.enableSource(sourceId);
    QVERIFY(manager.isSourceEnabled(sourceId));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
}

void SkyCatalogManagerRelatedDataTests::lateRelatedFailureStatusFromSupersededOwnerIsIgnored()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.error = QNetworkReply::HostNotFoundError,
         .errorText = QStringLiteral("No fake response registered"),
         .httpStatusCode = 404,
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    manager.disableSource(sourceId);
    const QString statusAfterDisable = manager.statusText();
    const int statusChangesAfterDisable = statusSpy.count();

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The failure status callback of the superseded request reports nothing.
    QCOMPARE(manager.statusText(), statusAfterDisable);
    QCOMPARE(statusSpy.count(), statusChangesAfterDisable);
}

void SkyCatalogManagerRelatedDataTests::outOfOrderRelatedRepliesPopulateTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/out-of-order-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/out-of-order-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/out-of-order-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/out-of-order-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
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
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> firstReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!firstReply.isNull());
    QVERIFY(!firstReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Source B's later request completes first; loading it did not discard
    // source A's pending response.
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    // Source A's earlier request completes after, into A's own record.
    QVERIFY(!firstReply.isNull());
    firstReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Each dataset stays with the owner whose response delivered it.
    manager.disableSource(sourceAId);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    manager.enableSource(sourceAId);
    manager.disableSource(sourceBId);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerRelatedDataTests::reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/pending-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/pending-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/pending-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/pending-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        FakeReplyQuery::findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    // Reloading and removing another source while the owner's response is
    // still pending must not supersede that response.
    manager.retrySource(sourceBId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.removeSource(sourceBId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceBId));
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    pendingReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerRelatedDataTests::reloadClearsOwnerRelatedDataBeforePublishing()
{
    const QString catalogUrl = QStringLiteral("https://example.test/reload-publish-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/reload-publish-lines.json");
    const QByteArray reloadedPayload =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 902001, .properName = "Reloaded Star", .mag = "2.0"});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = reloadedPayload});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    // The reload is accepted while its replacement related reply is still
    // pending. The notification must already carry the cleared related state
    // that the runtime keeps from then on, not the dataset of the replaced
    // catalog, and that state must stay published until the next signal.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);

    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Star")));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);

    QCoreApplication::processEvents();
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Persisting during the pending interval keeps the accepted declaration
    // and the cleared dataset together, so a restart restores that same
    // intended state instead of the discarded references.
    {
        const auto pendingSnapshot = store.loadCatalogCollectionCache();
        QVERIFY(pendingSnapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record =
            CatalogSourceFactsQuery::findRecord(pendingSnapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(record->constellationLineRows.isEmpty());
        QVERIFY(record->constellationAnchorGroupRows.isEmpty());
        QCOMPARE(record->payload, reloadedPayload);
    }
    {
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
        QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Reloaded Star")));
    }

    // The accepted replacement publishes its own dataset in a further
    // notification that again matches the runtime state.
    skygate::ui::tests::FakeNetworkReply* replacementReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(replacementReply != nullptr);
    replacementReply->finishNow();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerRelatedDataTests::failedRelatedReplacementKeepsPublishedStateSynchronized()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.error = QNetworkReply::ContentNotFoundError,
         .errorText = QStringLiteral("Not Found"),
         .httpStatusCode = 404,
         .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = QByteArray("not a constellation payload\n"), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    // The replacement download fails with an HTTP error. The failure cannot
    // change the related view already published with the reload, so it emits
    // no further catalogChanged and the observer's view stays valid.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    const int changesBeforeHttpFailure = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* failedReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(failedReply != nullptr);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/failed-replacement-lines\\.json .* HTTP 404")
    );
    failedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeHttpFailure);
    QVERIFY(manager.constellationLineRefs().empty());
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // A replacement payload that cannot be parsed keeps the same cleared
    // state and likewise emits no further catalogChanged.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    const int changesBeforeParseFailure = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* unparsableReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(unparsableReply != nullptr);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Constellation line parse failed"));
    unparsableReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeParseFailure);
    QVERIFY(manager.constellationLineRefs().empty());
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Persistence carries the same cleared state.
    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerRelatedDataTests::canceledRelatedReplacementKeepsPublishedStateSynchronized()
{
    const QString catalogUrl = QStringLiteral("https://example.test/cancel-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/cancel-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Canceling supersedes the pending replacement without changing the
    // related view that was published with the accepted reload.
    const auto revisionBeforeCancel = manager.catalogRevision();
    const int changesBeforeCancel = catalogSpy.count();
    manager.cancelCatalogDownload();
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QVERIFY(manager.constellationLineRefs().empty());
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // The canceled reply cannot attach anything afterwards.
    QTRY_VERIFY(allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QVERIFY(manager.constellationLineRefs().empty());
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerRelatedDataTests::reloadClearsOnlyOwningRelatedDataset()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/scoped-reload-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/scoped-reload-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/scoped-reload-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/scoped-reload-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceBRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl,
        {.payload = RelatedDatasetFixture::relatedDatasetPayload(QStringLiteral("corona"), {27989, 25336, 25930}),
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        RelatedDatasetFixture::relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        RelatedDatasetFixture::relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    // Reloading source A clears only A's own dataset; the completed sibling
    // dataset and the published notification state stay untouched.
    manager.retrySource(sourceAId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, sourceARelatedUrl) != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // A's replacement then joins the sibling's retained dataset.
    skygate::ui::tests::FakeNetworkReply* replacementReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(replacementReply != nullptr);
    replacementReply->finishNow();
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Corona") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationCount(), std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerRelatedDataTests::relatedDatasetsRoundTripToTheirOwnSources()
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
    const SkySettingsStore::CatalogSourceCacheRecord* recordA =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceBId);
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

void SkyCatalogManagerRelatedDataTests::disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart()
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
    const SkySettingsStore::CatalogSourceCacheRecord* recordA =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sourceBId);
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

void SkyCatalogManagerRelatedDataTests::removedOwnerRelatedDataDoesNotReturnAfterRestart()
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
    const SkySettingsStore::CatalogSourceCacheRecord* survivingRecord =
        CatalogSourceFactsQuery::findRecord(afterRemoval.snapshot, sourceAId);
    QVERIFY(survivingRecord != nullptr);
    const auto survivingLines = CatalogSourceFactsQuery::relatedLineRefs(*survivingRecord);
    QVERIFY(!CatalogSourceFactsQuery::relatedLineRefsContainHip(survivingLines, "hip_26311"));

    // The retired pre-collection two-slot cache still holds related data on
    // disk, but a committed collection snapshot keeps that fallback unused.
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Related");
    legacy.catalogPayload = skygate::ui::tests::orionHygCsvPayload();
    legacy.constellationLineRows = "hip_26311|hip_26727\n";
    legacy.constellationAnchorGroupRows = "Lyra|hip_26311,hip_26727\n";
    legacy.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
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

void SkyCatalogManagerRelatedDataTests::corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact()
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
        [&sourceAId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == sourceAId;
        }
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

void SkyCatalogManagerRelatedDataTests::migratesPriorSingleOwnerRelatedPayloadOnce()
{
    // A snapshot written before the per-source related format: the bundled
    // record and one star record, where the star record holds the only copy of
    // the then-collection-wide related dataset.
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion - 1;

    SkySettingsStore::CatalogSourceCacheRecord bundled;
    bundled.instanceId = QStringLiteral("primary");
    bundled.title = QStringLiteral("Bundled");
    bundled.bundled = true;
    bundled.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bundled.enabled = true;
    bundled.order = 0;
    snapshot.sources.push_back(std::move(bundled));

    SkySettingsStore::CatalogSourceCacheRecord star;
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
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
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
        migrated.snapshot.schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    const SkySettingsStore::CatalogSourceCacheRecord* migratedRecord =
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

void SkyCatalogManagerRelatedDataTests::updateRemovingRelatedDeclarationRetiresOwnedData()
{
    const QString catalogUrl = QStringLiteral("https://example.test/removed-declaration-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/removed-declaration-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/removed-declaration-update.csv");
    const QByteArray updatePayload =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 902101, .properName = "No Related Update Star", .mag = "2.0"});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    // The accepted update declares no related dataset anymore. The obsolete
    // owned data retires in the same accepted transition, before publication.
    skygate::ui::internal::SkyCatalogSourceInstance updated = source;
    updated.title = QStringLiteral("No Related Update");
    updated.version = QStringLiteral("v2");
    updated.urls = QStringList{updateUrl};
    updated.relatedDatasetUrls.clear();
    manager.loadSource(updated, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("No Related Update Star")));
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // The persisted snapshot carries the accepted declaration and retired
    // rows together, so a restart cannot resurrect the retired dataset.
    {
        const auto snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record =
            CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QVERIFY(record->relatedDatasetUrls.isEmpty());
        QVERIFY(record->constellationLineRows.isEmpty());
        QVERIFY(record->constellationAnchorGroupRows.isEmpty());
        QCOMPARE(record->payload, updatePayload);
    }
    {
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
        QVERIFY(
            CatalogBodyQuery::containsDisplayName(
                restoredManager.starCatalog(), QStringLiteral("No Related Update Star")
            )
        );

        // Disabling and re-enabling cannot resurrect retired owned data: no
        // related declaration exists to request it from.
        networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
        restoredManager.disableSource(instanceId);
        restoredManager.enableSource(instanceId);
        QCoreApplication::processEvents();
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
    }
}

void SkyCatalogManagerRelatedDataTests::failedUpdateAttemptingToRemoveRelatedDeclarationKeepsDataset()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-removal-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-removal-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/failed-removal-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.error = QNetworkReply::ContentNotFoundError, .errorText = QStringLiteral("Not Found"), .httpStatusCode = 404}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    const auto starCountBefore = manager.bodyCount();

    // The update that would have removed the declaration is rejected, so the
    // accepted source and its owned dataset stay coherent and published.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = source;
    rejected.title = QStringLiteral("Rejected Removal");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.relatedDatasetUrls.clear();
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QCOMPARE(manager.bodyCount(), starCountBefore);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Persistence still serializes the accepted declaration and rows.
    manager.moveSource(instanceId, 0);
    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerRelatedDataTests::lateReplyToRemovedRelatedDeclarationIsRejected()
{
    const QString catalogUrl = QStringLiteral("https://example.test/late-removal-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/late-removal-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/late-removal-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 902102, .properName = "Late Update Star"})}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);

    // The update retires the declaration while the old related reply is still
    // pending. The accepted transition must not keep the old dataset active.
    skygate::ui::internal::SkyCatalogSourceInstance updated = source;
    updated.version = QStringLiteral("v2");
    updated.urls = QStringList{updateUrl};
    updated.relatedDatasetUrls.clear();
    manager.loadSource(updated, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());

    // The late reply belongs to the superseded revision and cannot attach
    // data or change the published related view.
    const int changesBeforeLateReply = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* lateReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(lateReply != nullptr);
    lateReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeLateReply);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Late Update Star")));
}

void SkyCatalogManagerRelatedDataTests::bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy_data()
{
    QTest::addColumn<int>("policy");
    QTest::addRow("merge") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTest::addRow("deep-sky-only") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTest::addRow("deep-sky-fallback") << static_cast<int>(
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyFallback
    );
    QTest::addRow("augment-core") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::AugmentCore);
}

void SkyCatalogManagerRelatedDataTests::bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy()
{
    QFETCH(int, policy);
    const auto bundledPolicy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(policy);

    const QString catalogUrl = QStringLiteral("https://example.test/bundled-policy-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/bundled-policy-lines.json");
    const QString siblingUrl = QStringLiteral("https://example.test/bundled-policy-sibling-stars.csv");
    const QString siblingRelatedUrl = QStringLiteral("https://example.test/bundled-policy-sibling-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(siblingUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        siblingRelatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    // A second owner keeps its own dataset throughout, so the retirement stays
    // scoped to the instance whose accepted declaration changed.
    const skygate::ui::internal::SkyCatalogSourceInstance sibling =
        RelatedDatasetFixture::relatedDatasetInstance(siblingUrl, siblingRelatedUrl);
    manager.loadSource(sibling, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);

    // The update keeps the durable instance ID but activates bundled content
    // whose declaration selects no related dataset.
    skygate::ui::internal::SkyCatalogSourceInstance update = source;
    update.title = QStringLiteral("Bundled Policy Update");
    update.version = QStringLiteral("v2");
    update.urls.clear();
    update.relatedDatasetUrls.clear();
    manager.loadSource(update, bundledPolicy);

    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") != nullptr);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{1});
    QCOMPARE(observations.back().constellationCount, std::size_t{1});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto updatedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(updatedEntry != view.end());
    QVERIFY(updatedEntry->bundled);
    QVERIFY(!updatedEntry->hasError);

    // The accepted declaration and the retired dataset are persisted together,
    // and the sibling record keeps its own payload.
    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* updatedRecord =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    const SkySettingsStore::CatalogSourceCacheRecord* siblingRecord =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, sibling.instanceId);
    QVERIFY(updatedRecord != nullptr);
    QVERIFY(siblingRecord != nullptr);
    QVERIFY(updatedRecord->relatedDatasetUrls.isEmpty());
    QVERIFY(updatedRecord->constellationLineRows.isEmpty());
    QVERIFY(updatedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(updatedRecord->bundled, true);
    QCOMPARE(siblingRecord->relatedDatasetUrls, QStringList{siblingRelatedUrl});
    QVERIFY(!siblingRecord->constellationLineRows.isEmpty());
    QVERIFY(!siblingRecord->constellationAnchorGroupRows.isEmpty());

    // A restart agrees with the live state instead of resurrecting the retired
    // dataset.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Orion") == nullptr);
    QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Lyra") != nullptr);
}

void SkyCatalogManagerRelatedDataTests::payloadOriginSwitchesFollowTheAcceptedRelatedDeclaration()
{
    const QString catalogUrl = QStringLiteral("https://example.test/origin-switch-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/origin-switch-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    // The instance starts as bundled content and owns no related dataset.
    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    source.title = QStringLiteral("Origin Switch Source");
    source.urls.clear();
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());

    // A downloaded update of the same instance declares its own related
    // dataset, which becomes owned once its own reply completes.
    skygate::ui::internal::SkyCatalogSourceInstance downloaded = source;
    downloaded.version = QStringLiteral("v2");
    downloaded.urls = QStringList{catalogUrl};
    downloaded.relatedDatasetUrls = QStringList{relatedUrl};
    manager.loadSource(downloaded, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    // Switching the same instance back to bundled content retires the dataset
    // exactly like a downloaded update that removes the declaration.
    skygate::ui::internal::SkyCatalogSourceInstance bundled = downloaded;
    bundled.version = QStringLiteral("v3");
    bundled.urls.clear();
    bundled.relatedDatasetUrls.clear();
    manager.loadSource(bundled, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->relatedDatasetUrls.isEmpty());
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(restoredManager.constellationLineRefs().empty());
    QVERIFY(restoredManager.constellationAnchorGroups().empty());
    QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
}

void SkyCatalogManagerRelatedDataTests::rejectedBundledUpdateKeepsOwnedRelatedData()
{
    const QString catalogUrl = QStringLiteral("https://example.test/rejected-bundled-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/rejected-bundled-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);

    // The bundled deep-sky fallback is disabled while the source loads, so its
    // reserved identity is available to the configured source. Re-enabling the
    // fallback later makes any transition of that source collide with the
    // implicit entry.
    manager.setDeepSkyCatalogPresetIndex(1);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    source.instanceId = QStringLiteral("bundled-deep-sky");
    source.title = QStringLiteral("Rejected Bundled Source");
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    manager.setDeepSkyCatalogPresetIndex(0);

    // The bundled update cannot compose against the enabled fallback, so the
    // replacement is rejected and the accepted dataset stays published.
    skygate::ui::internal::SkyCatalogSourceInstance update = source;
    update.title = QStringLiteral("Rejected Bundled Update");
    update.version = QStringLiteral("v2");
    update.urls.clear();
    update.relatedDatasetUrls.clear();
    manager.loadSource(update, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto updatedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(updatedEntry != view.end());
    QVERIFY(updatedEntry->hasError);
    QVERIFY(updatedEntry->statusText.contains(QStringLiteral("rejected")));
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // The rejected attempt leaves the accepted declaration and its dataset in
    // the persisted record.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(record->bundled, false);
}

QTEST_GUILESS_MAIN(SkyCatalogManagerRelatedDataTests)

#include "SkyCatalogManagerRelatedDataTests.moc"
