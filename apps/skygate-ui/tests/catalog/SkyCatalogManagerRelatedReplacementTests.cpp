#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogBodyQuery.hpp"
#include "support/CatalogNotificationRecorder.hpp"
#include "support/CatalogSourceFactsQuery.hpp"
#include "support/CatalogTestPayloads.hpp"
#include "support/ConstellationAnchorGroupLookup.hpp"
#include "support/ConstellationTestSupport.hpp"
#include "support/FakeNetworkAccessManager.hpp"
#include "support/FakeReplyQuery.hpp"
#include "support/RelatedDatasetFixture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>
#include <vector>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogNotificationRecorder;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

class SkyCatalogManagerRelatedReplacementTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void reloadClearsOwnerRelatedDataBeforePublishing();
    void failedReplacementKeepsPublishedStateInSync();
    void canceledReplacementKeepsPublishedStateInSync();
    void reloadClearsOnlyOwningRelatedDataset();
    void updateRemovingRelatedDeclarationRetiresOwnedData();
    void failedUpdateRemovingDeclarationKeepsDataset();
    void bundledUpdateRetiresOwnedDataEveryPolicy_data();
    void bundledUpdateRetiresOwnedDataEveryPolicy();
    void payloadOriginSwitchesFollowAcceptedDeclaration();
    void rejectedBundledUpdateKeepsOwnedRelatedData();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerRelatedReplacementTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerRelatedReplacementTests")));
}

void SkyCatalogManagerRelatedReplacementTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerRelatedReplacementTests::reloadClearsOwnerRelatedDataBeforePublishing()
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
        const SkyCatalogSourceCacheRecord* record =
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

void SkyCatalogManagerRelatedReplacementTests::failedReplacementKeepsPublishedStateInSync()
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
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerRelatedReplacementTests::canceledReplacementKeepsPublishedStateInSync()
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
    QTRY_VERIFY(FakeReplyQuery::allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QVERIFY(manager.constellationLineRefs().empty());
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerRelatedReplacementTests::reloadClearsOnlyOwningRelatedDataset()
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

void SkyCatalogManagerRelatedReplacementTests::updateRemovingRelatedDeclarationRetiresOwnedData()
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
        const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
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

void SkyCatalogManagerRelatedReplacementTests::failedUpdateRemovingDeclarationKeepsDataset()
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
    const auto starCountBefore = manager.starCatalog()->bodies().size();

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
    QCOMPARE(manager.starCatalog()->bodies().size(), starCountBefore);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Persistence still serializes the accepted declaration and rows.
    manager.moveSource(instanceId, 0);
    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerRelatedReplacementTests::bundledUpdateRetiresOwnedDataEveryPolicy_data()
{
    QTest::addColumn<int>("policy");
    QTest::addRow("merge") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTest::addRow("deep-sky-only") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTest::addRow("deep-sky-fallback") << static_cast<int>(
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyFallback
    );
    QTest::addRow("augment-core") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::AugmentCore);
}

void SkyCatalogManagerRelatedReplacementTests::bundledUpdateRetiresOwnedDataEveryPolicy()
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
    const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* updatedRecord =
        CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    const SkyCatalogSourceCacheRecord* siblingRecord =
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

void SkyCatalogManagerRelatedReplacementTests::payloadOriginSwitchesFollowAcceptedDeclaration()
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

    const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
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

void SkyCatalogManagerRelatedReplacementTests::rejectedBundledUpdateKeepsOwnedRelatedData()
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
    const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(record->bundled, false);
}

QTEST_GUILESS_MAIN(SkyCatalogManagerRelatedReplacementTests)

#include "SkyCatalogManagerRelatedReplacementTests.moc"
