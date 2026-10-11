#include "CatalogBodyQuery.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
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
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

namespace {

constexpr int kStaleConstellationDelayMs = 500;

const skygate::ui::internal::SkyCatalogSourceDescriptor kHygPreset =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

}  // namespace

class SkyCatalogManagerRelatedReplyTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void staleConstellationResponseAfterBundledSwitch();
    void staleConstellationResponseAfterCustomSwitch();
    void cancelDuringConstellationLoadIgnoresStaleReply();
    void currentConstellationResponseAppliesOnce();
    void failedUpdateSupersedesItsPendingRelatedResponse();
    void relatedDatasetsStayOwnedByTheirSources();
    void overlappingDatasetsFollowSourceOrder();
    void lateRelatedResponseAfterOwnerRemovalIsIgnored();
    void readdedOwnerRejectsPreviousIncarnationResponse();
    void disablingOwnerSupersedesPendingRelatedReply();
    void lateRelatedFailureFromSupersededOwnerIgnored();
    void outOfOrderRelatedRepliesPopulateTheirOwnSources();
    void reloadOrRemoveOtherSourceKeepsPendingReply();
    void lateReplyToRemovedRelatedDeclarationIsRejected();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerRelatedReplyTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerRelatedReplyTests")));
}

void SkyCatalogManagerRelatedReplyTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerRelatedReplyTests::staleConstellationResponseAfterBundledSwitch()
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

void SkyCatalogManagerRelatedReplyTests::staleConstellationResponseAfterCustomSwitch()
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

void SkyCatalogManagerRelatedReplyTests::cancelDuringConstellationLoadIgnoresStaleReply()
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

    // Cancellation aborts the pending transfer without requesting the next
    // candidate URL.
    QTRY_VERIFY(FakeReplyQuery::allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();

    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({catalogUrl, constellationUrl}));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({constellationUrl}));
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), catalogChangesBeforeCancel);
}

void SkyCatalogManagerRelatedReplyTests::currentConstellationResponseAppliesOnce()
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

void SkyCatalogManagerRelatedReplyTests::failedUpdateSupersedesItsPendingRelatedResponse()
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
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Superseded Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(record->constellationLineRows.isEmpty());
}

void SkyCatalogManagerRelatedReplyTests::relatedDatasetsStayOwnedByTheirSources()
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

void SkyCatalogManagerRelatedReplyTests::overlappingDatasetsFollowSourceOrder()
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

void SkyCatalogManagerRelatedReplyTests::lateRelatedResponseAfterOwnerRemovalIsIgnored()
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

void SkyCatalogManagerRelatedReplyTests::readdedOwnerRejectsPreviousIncarnationResponse()
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
        [&sourceId](const SkyCatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
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
        [&sourceId](const SkyCatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(staleRecord != persistedAfterStale.snapshot.sources.end());
    QCOMPARE(staleRecord->constellationLineRows, persistedLineRows);
}

void SkyCatalogManagerRelatedReplyTests::disablingOwnerSupersedesPendingRelatedReply()
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

void SkyCatalogManagerRelatedReplyTests::lateRelatedFailureFromSupersededOwnerIgnored()
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

void SkyCatalogManagerRelatedReplyTests::outOfOrderRelatedRepliesPopulateTheirOwnSources()
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

void SkyCatalogManagerRelatedReplyTests::reloadOrRemoveOtherSourceKeepsPendingReply()
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

void SkyCatalogManagerRelatedReplyTests::lateReplyToRemovedRelatedDeclarationIsRejected()
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

QTEST_GUILESS_MAIN(SkyCatalogManagerRelatedReplyTests)

#include "SkyCatalogManagerRelatedReplyTests.moc"
