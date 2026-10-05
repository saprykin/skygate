#include "CatalogCacheTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"

#include <QDir>
#include <QFile>
#include <QList>
#include <QLocale>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool catalogContainsDisplayName(const skygate::ephemeris::IStarCatalog* catalog, const QString& displayName)
{
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&displayName](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->displayName) == displayName;
    });
}

bool catalogContainsId(const skygate::ephemeris::IStarCatalog* catalog, const QString& objectId)
{
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&objectId](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->id) == objectId;
    });
}

constexpr const char* kArchiveStarsMember = "catalog/stars.csv";
constexpr const char* kArchiveDeepSkyMember = "catalog/deep-sky.csv";

// A two-member archive whose members hold distinguishable objects, so a test
// can tell which member a restored source actually selected.
QByteArray archiveMemberZip(const skygate::ui::tests::DeepSkyCatalogPayloadOptions& deepSky)
{
    const QByteArray starsMember =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 900101, .properName = "Archive Member Star", .mag = "1.0"});
    const QByteArray deepSkyMember = skygate::ui::tests::sampleOpenNgcCsvPayload(deepSky);

    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{.path = kArchiveStarsMember, .data = starsMember.toStdString()},
        skygate::ephemeris::tests::ZipEntrySpec{.path = kArchiveDeepSkyMember, .data = deepSkyMember.toStdString()},
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

constexpr int kStaleConstellationDelayMs = 500;

// A source instance configured with one related constellation dataset, so a
// test can drive the owner-bound related-data lifecycle with fake replies.
skygate::ui::internal::SkyCatalogSourceInstance
relatedDatasetInstance(const QString& catalogUrl, const QString& relatedUrl)
{
    skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    instance.relatedDatasetUrls = QStringList{relatedUrl};
    return instance;
}

// One related constellation dataset with a single two-segment constellation,
// distinguishable per owner by its constellation id and anchor name.
QByteArray relatedDatasetPayload(const QString& constellationId, const QList<int>& hips)
{
    return skygate::ui::tests::stellariumConstellationIndexJsonPayload({{constellationId, hips}});
}

QByteArray orionRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("orion"), {27989, 25336, 25930});
}

QByteArray lyraRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("lyra"), {26311, 26727, 24436});
}

bool collectionContainsInstanceId(
    const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId
)
{
    return std::any_of(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
}

const SkySettingsStore::CatalogSourceCacheRecord*
findCollectionRecord(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId)
{
    const auto it = std::find_if(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    return it != snapshot.sources.end() ? &*it : nullptr;
}

std::vector<skygate::ephemeris::ConstellationLineRef>
relatedLineRefs(const SkySettingsStore::CatalogSourceCacheRecord& record)
{
    return skygate::ui::internal::SkyContextCatalogCodec::parseConstellationLineRows(
        std::string_view(
            record.constellationLineRows.constData(), static_cast<std::size_t>(record.constellationLineRows.size())
        )
    );
}

bool relatedLineRefsContainHip(
    const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs, const std::string& hipId
)
{
    return std::any_of(
        lineRefs.begin(), lineRefs.end(), [&hipId](const skygate::ephemeris::ConstellationLineRef& lineRef) {
            return lineRef.first == hipId || lineRef.second == hipId;
        }
    );
}

// Finds the active related anchor group by constellation name. The returned
// pointer stays valid until the next active-view mutation.
const skygate::ephemeris::ConstellationAnchorGroup*
findConstellationAnchorGroup(const SkyCatalogManager& manager, const std::string& name)
{
    for (const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup : manager.constellationAnchorGroups()) {
        if (anchorGroup.first == name) {
            return &anchorGroup;
        }
    }
    return nullptr;
}

const skygate::ui::internal::SkyCatalogSourceDescriptor kHygPreset =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

skygate::ui::tests::FakeNetworkReply*
findReplyForUrl(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager, const QString& url)
{
    for (skygate::ui::tests::FakeNetworkReply* reply : networkAccessManager.issuedReplies()) {
        if (reply != nullptr && reply->url().toString() == url) {
            return reply;
        }
    }
    return nullptr;
}

bool allIssuedRepliesFinished(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager)
{
    const auto replies = networkAccessManager.issuedReplies();
    return std::all_of(replies.begin(), replies.end(), [](const skygate::ui::tests::FakeNetworkReply* reply) {
        return reply == nullptr || reply->isFinished();
    });
}

}  // namespace

class SkyCatalogManagerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void unknownPresetsUpdateStatus();
    void bundledPresetResetsCatalogAndConstellationRefs();
    void bundledReplacementDropsOwnerDataAndReportsFreshCounts();
    void bundledDeepSkyPresetRebuildsActiveCatalog();
    void customDeepSkyDownloadUsesGenericSourceLabel();
    void clearCacheReportsStatusAndSignals();
    void restoreCachePathThroughManager();
    void localCatalogDownloadTogglesBusyProcessingAndAppliesCatalog();
    void cancelCatalogDownloadClearsBusyAndIgnoresResult();
    void failedLocalCatalogDownloadClearsBusyAndReportsStatus();
    void staleConstellationResponseIgnoredAfterBundledSwitch();
    void staleConstellationResponseIgnoredAfterCustomSwitch();
    void cancelDuringConstellationLoadingIgnoresStaleCompletion();
    void currentConstellationResponseAppliesOnce();
    void collectionSourcesLoadEnableDisableAndRemoveIndependently();
    void failedCollectionLoadPreservesPriorDataAndRetrySucceeds();
    void addSourceUrlAddsSameCategorySourcesWithStableIdentity();
    void addSourcePresetUsesDescriptorPolicy();
    void moveSourceReordersActiveSources();
    void clearSourceCacheRemovesSingleRecord();
    void sameDescriptorInstancesCoexistIndependently();
    void sameUrlDifferentVersionsStayDistinct();
    void editingSourceAsUpdatePreservesInstanceId();
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
    void relatedConstellationDatasetsStayOwnedByTheirSources();
    void overlappingConstellationDatasetsFollowSourceOrder();
    void lateRelatedResponseAfterOwnerRemovalIsIgnored();
    void readdedOwnerRejectsPreviousIncarnationResponse();
    void disablingOwnerSupersedesItsPendingRelatedResponse();
    void lateRelatedFailureStatusFromSupersededOwnerIsIgnored();
    void outOfOrderRelatedRepliesPopulateTheirOwnSources();
    void reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse();
    void relatedDatasetsRoundTripToTheirOwnSources();
    void disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart();
    void removedOwnerRelatedDataDoesNotReturnAfterRestart();
    void corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact();
    void migratesPriorSingleOwnerRelatedPayloadOnce();
    void presentationSummarizesEnabledCollectionParticipation();
    void bundledFallbackPresentationFollowsParticipationAndRestart();

private:
    SkySettingsStore::CatalogCacheSnapshot makeCacheSnapshot() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerTests")));
}

void SkyCatalogManagerTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogManagerTests::makeCacheSnapshot() const
{
    return skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
}

void SkyCatalogManagerTests::unknownPresetsUpdateStatus()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset("unknown");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown preset 'unknown'"));
    QCOMPARE(statusSpy.count(), 1);

    manager.loadDeepSkyCatalogPreset("unknown_dso");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown deep-sky preset 'unknown_dso'"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::bundledPresetResetsCatalogAndConstellationRefs()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.setCatalogPresetIndex(99);
    manager.loadCatalogPreset("bundled");

    QCOMPARE(manager.catalogPresetIndex(), 0);
    QCOMPARE(manager.sourceLabel(), QString("Bundled"));
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(catalogSpy.count() >= 1);
}

void SkyCatalogManagerTests::bundledReplacementDropsOwnerDataAndReportsFreshCounts()
{
    const QString catalogUrl = QStringLiteral("https://example.test/bundled-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/bundled-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    // The legacy primary slot owns a related dataset before the bundled preset
    // replaces its catalog.
    skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    source.instanceId = QStringLiteral("primary");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationCount() > 0U);

    manager.loadCatalogPreset("bundled");

    // The bundled replacement drops the owned related dataset, and the status
    // text of that committed replacement reports the post-clear counts instead
    // of the pre-clear ones.
    QVERIFY(manager.constellationLineRefs().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().endsWith(
        QStringLiteral("%1 constellations)").arg(locale.toString(static_cast<qulonglong>(manager.constellationCount())))
    ));
}

void SkyCatalogManagerTests::bundledDeepSkyPresetRebuildsActiveCatalog()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy infoSpy(&manager, &SkyCatalogManager::deepSkyCatalogInfoTextChanged);

    manager.setDeepSkyCatalogPresetIndex(99);
    manager.loadDeepSkyCatalogPreset("bundled_messier");

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 0);
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.deepSkyCatalogInfoText().contains("Objects:"));
    QVERIFY(infoSpy.count() >= 1);
}

void SkyCatalogManagerTests::customDeepSkyDownloadUsesGenericSourceLabel()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-deep-sky.csv"));
    QVERIFY(writeFile(catalogPath, skygate::ui::tests::sampleOpenNgcCsvPayload()));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    manager.downloadDeepSkyCatalogFromUrl(catalogUrl);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(manager.deepSkyCatalogUrlText(), catalogUrl);
    QVERIFY(manager.sourceTitles().values().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.sourceTitles().values().contains(QStringLiteral("OpenNGC")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.statusText().contains(QStringLiteral("OpenNGC")));
}

void SkyCatalogManagerTests::clearCacheReportsStatusAndSignals()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    QVERIFY(manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Star catalog cache cleared"));
    QVERIFY(manager.clearDeepSkyCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Deep-sky catalog cache cleared"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::restoreCachePathThroughManager()
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

void SkyCatalogManagerTests::localCatalogDownloadTogglesBusyProcessingAndAppliesCatalog()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-stars.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload({.hip = 900001, .properName = "Manager Downloaded Star", .mag = "1.0"})
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy processingSpy(&manager, &SkyCatalogManager::catalogProcessingChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.downloadCatalogFromUrl(QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.downloadingCatalog());
    QVERIFY(!manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Cannot clear cache while download is in progress"));

    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.catalogProcessing());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QString("Manager Downloaded Star")));
    QCOMPARE(manager.catalogPresetIndex(), 2);
    QCOMPARE(manager.catalogUrlText(), QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(downloadSpy.count() >= 2);
    QVERIFY(processingSpy.count() >= 2);
    QVERIFY(catalogSpy.count() >= 1);
    QVERIFY(statusSpy.count() >= 2);
}

void SkyCatalogManagerTests::cancelCatalogDownloadClearsBusyAndIgnoresResult()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-cancel-stars.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload({
            .hip = 900002,
            .properName = "Canceled Manager Star",
            .mag = "1.0",
        })
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.downloadCatalogFromUrl(QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> busyView = manager.sourceViewEntries();
    QVERIFY(!busyView.isEmpty());
    QVERIFY(busyView.first().busy);

    manager.cancelCatalogDownload();

    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(manager.statusText(), QString("Catalog: Download canceled."));

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = manager.sourceViewEntries();
    QVERIFY(!canceledView.isEmpty());
    QVERIFY(!canceledView.first().busy);
    QCOMPARE(canceledView.first().statusText, QStringLiteral("Canceled"));

    QCoreApplication::processEvents();
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QString("Canceled Manager Star")));
    QVERIFY(downloadSpy.count() >= 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerTests::failedLocalCatalogDownloadClearsBusyAndReportsStatus()
{
    const QString missingCatalogUrl =
        QUrl::fromLocalFile(m_settings.filePath(QStringLiteral("missing-stars.csv"))).toString();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const std::uint64_t originalRevision = manager.catalogRevision();
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed file://.*/missing-stars\\.csv .* HTTP 0")
    );
    manager.downloadCatalogFromUrl(missingCatalogUrl);
    QVERIFY(manager.downloadingCatalog());

    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(manager.statusText().contains(QStringLiteral("failed"), Qt::CaseInsensitive));
    QVERIFY(manager.statusText().contains(missingCatalogUrl));
    QCOMPARE(manager.catalogRevision(), originalRevision);
    QCOMPARE(downloadSpy.count(), 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerTests::staleConstellationResponseIgnoredAfterBundledSwitch()
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
        findReplyForUrl(networkAccessManager, constellationUrl);
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

void SkyCatalogManagerTests::staleConstellationResponseIgnoredAfterCustomSwitch()
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
        findReplyForUrl(networkAccessManager, constellationUrl);
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
    QVERIFY(cacheSnapshot.has_value());
    QCOMPARE(cacheSnapshot->sources.size(), 1);
    QVERIFY(cacheSnapshot->sources[0].constellationLineRows.isEmpty());
    QVERIFY(cacheSnapshot->sources[0].constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerTests::cancelDuringConstellationLoadingIgnoresStaleCompletion()
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

void SkyCatalogManagerTests::currentConstellationResponseAppliesOnce()
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

void SkyCatalogManagerTests::collectionSourcesLoadEnableDisableAndRemoveIndependently()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    const QString sourceAUrl = writeSourceFile(QStringLiteral("collection-a.csv"), "Collection Star A", 901001);
    const QString sourceBUrl = writeSourceFile(QStringLiteral("collection-b.csv"), "Collection Star B", 901002);
    const QString sourceCUrl = writeSourceFile(QStringLiteral("collection-c.csv"), "Collection Star C", 901003);
    QVERIFY(!sourceAUrl.isEmpty());
    QVERIFY(!sourceBUrl.isEmpty());
    QVERIFY(!sourceCUrl.isEmpty());

    loadLocalSource(sourceAUrl);
    const QString sourceAId = manager.sourceInstanceIds().last();
    loadLocalSource(sourceBUrl);
    loadLocalSource(sourceCUrl);

    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.disableSource(sourceAId);
    QVERIFY(!manager.isSourceEnabled(sourceAId));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.enableSource(sourceAId);
    QVERIFY(manager.isSourceEnabled(sourceAId));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));

    manager.removeSource(sourceAId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));
}

void SkyCatalogManagerTests::failedCollectionLoadPreservesPriorDataAndRetrySucceeds()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString sourceAPath = m_settings.filePath(QStringLiteral("retry-source-a.csv"));
    QVERIFY(writeFile(
        sourceAPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 901010, .hip = 901010, .properName = "Retry Source A", .mag = "1.0"}
        )
    ));
    const QString sourceAUrl = QUrl::fromLocalFile(sourceAPath).toString();

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    loadLocalSource(sourceAUrl);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    const std::uint64_t revisionAfterA = manager.catalogRevision();

    const QString retryPath = m_settings.filePath(QStringLiteral("retry-source-b.csv"));
    const QString retryUrl = QUrl::fromLocalFile(retryPath).toString();

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed file://.*/retry-source-b\\.csv .* HTTP 0")
    );
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(retryUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto retryEntry =
        std::find_if(failedView.begin(), failedView.end(), [](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.hasError;
        });
    QVERIFY(retryEntry != failedView.end());
    const QString retrySourceId = retryEntry->instanceId;

    // A failed source operation leaves the prior valid active data untouched.
    QCOMPARE(manager.catalogRevision(), revisionAfterA);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{2});

    QVERIFY(writeFile(
        retryPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 901011, .hip = 901011, .properName = "Retry Source B", .mag = "1.0"}
        )
    ));
    manager.retrySource(retrySourceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{3});
}

void SkyCatalogManagerTests::addSourceUrlAddsSameCategorySourcesWithStableIdentity()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString firstUrl = writeSourceFile(QStringLiteral("same-category-a.csv"), "Same Category A", 902001);
    const QString secondUrl = writeSourceFile(QStringLiteral("same-category-b.csv"), "Same Category B", 902002);
    QVERIFY(!firstUrl.isEmpty());
    QVERIFY(!secondUrl.isEmpty());

    manager.addSourceUrl(firstUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(secondUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Same Category A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Same Category B")));

    QVERIFY(firstId != secondId);
    QVERIFY(manager.sourceInstanceIds().contains(firstId));
    QVERIFY(manager.sourceInstanceIds().contains(secondId));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    QCOMPARE(view.size(), 3);
    QCOMPARE(view[1].instanceId, firstId);
    QCOMPARE(view[2].instanceId, secondId);
    QCOMPARE(view[1].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(view[2].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

void SkyCatalogManagerTests::addSourcePresetUsesDescriptorPolicy()
{
    const QString hygUrl = kHygPreset.urls.value(0);
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        hygUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 902010, .properName = "Preset HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0998", .ngc = "0998", .identifiers = "PGC 9998", .commonName = "Preset Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString hygInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(hygInstanceId));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Preset HYG Star")));

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString openNgcInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(openNgcInstanceId));
    QVERIFY(hygInstanceId != openNgcInstanceId);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findViewEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto hygView = findViewEntry(hygInstanceId);
    const auto openNgcView = findViewEntry(openNgcInstanceId);
    QVERIFY(hygView != view.end());
    QVERIFY(openNgcView != view.end());
    QCOMPARE(hygView->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(openNgcView->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

void SkyCatalogManagerTests::moveSourceReordersActiveSources()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString aUrl = writeSourceFile(QStringLiteral("move-a.csv"), "Move Star A", 902020);
    const QString bUrl = writeSourceFile(QStringLiteral("move-b.csv"), "Move Star B", 902021);
    QVERIFY(!aUrl.isEmpty());
    QVERIFY(!bUrl.isEmpty());

    manager.addSourceUrl(aUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString aId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(bUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString bId = manager.sourceInstanceIds().last();
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), aId, bId}));

    manager.moveSource(bId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
}

void SkyCatalogManagerTests::clearSourceCacheRemovesSingleRecord()
{
    const QString aPath = m_settings.filePath(QStringLiteral("clear-a.csv"));
    const QString bPath = m_settings.filePath(QStringLiteral("clear-b.csv"));
    QVERIFY(writeFile(
        aPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 902030, .hip = 902030, .properName = "Clear Star A", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        bPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 902031, .hip = 902031, .properName = "Clear Star B", .mag = "1.0"}
        )
    ));

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

    QVERIFY(manager.clearSourceCache(aId));
    const auto cacheAfterClear = store.loadCatalogCollectionCache();
    QVERIFY(cacheAfterClear.has_value());
    // The bundled source and the peer downloaded source stay configured; only
    // the cleared source's record and payload are gone.
    QCOMPARE(cacheAfterClear->sources.size(), 2);
    QCOMPARE(cacheAfterClear->sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(cacheAfterClear->sources[0].bundled);
    QCOMPARE(cacheAfterClear->sources[1].instanceId, bId);
}

void SkyCatalogManagerTests::sameDescriptorInstancesCoexistIndependently()
{
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0991",
              .type = "G",
              .ra = "00:20:00.00",
              .dec = "+10:00:00.0",
              .messier = "",
              .ngc = "0991",
              .identifiers = "PGC 9991",
              .commonName = "First Galaxy"}
         )}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0992",
              .type = "G",
              .ra = "00:30:00.00",
              .dec = "+11:00:00.0",
              .messier = "",
              .ngc = "0992",
              .identifiers = "PGC 9992",
              .commonName = "Second Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QVERIFY(firstId != secondId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance participates independently.
    manager.disableSource(firstId);
    QVERIFY(!manager.isSourceEnabled(firstId));
    QVERIFY(manager.isSourceEnabled(secondId));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    manager.enableSource(firstId);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reloads independently.
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0993",
              .type = "G",
              .ra = "00:35:00.00",
              .dec = "+11:30:00.0",
              .messier = "",
              .ngc = "0993",
              .identifiers = "PGC 9993",
              .commonName = "Reloaded Galaxy"}
         )}
    );
    manager.retrySource(firstId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), firstId, secondId}));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reorders independently.
    manager.moveSource(secondId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));

    // Restart restores the bundled source and both instances with their
    // durable IDs and order.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance removes independently.
    restoredManager.removeSource(firstId);
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId}));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
}

void SkyCatalogManagerTests::sameUrlDifferentVersionsStayDistinct()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("versioned-star.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903001, .hip = 903001, .properName = "Versioned Star", .mag = "1.0"}
        )
    ));
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance first =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v1"));
    first.archiveSelector = QStringLiteral("members/catalog-v1.csv");
    skygate::ui::internal::SkyCatalogSourceInstance second =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v2"));
    second.archiveSelector = QStringLiteral("members/catalog-v2.csv");

    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(first.instanceId != second.instanceId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto firstEntry = findEntry(first.instanceId);
    const auto secondEntry = findEntry(second.instanceId);
    QVERIFY(firstEntry != view.end());
    QVERIFY(secondEntry != view.end());
    QCOMPARE(firstEntry->version, QStringLiteral("v1"));
    QCOMPARE(secondEntry->version, QStringLiteral("v2"));

    const std::optional<SkySettingsStore::CatalogCollectionCacheSnapshot> snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->sources.size(), 3);
    QCOMPARE(snapshot->sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot->sources[0].bundled);
    QCOMPARE(snapshot->sources[1].instanceId, first.instanceId);
    QCOMPARE(snapshot->sources[1].version, QStringLiteral("v1"));
    QCOMPARE(snapshot->sources[1].archiveSelector, QStringLiteral("members/catalog-v1.csv"));
    QCOMPARE(snapshot->sources[2].instanceId, second.instanceId);
    QCOMPARE(snapshot->sources[2].version, QStringLiteral("v2"));
    QCOMPARE(snapshot->sources[2].archiveSelector, QStringLiteral("members/catalog-v2.csv"));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), first.instanceId, second.instanceId})
    );
}

void SkyCatalogManagerTests::editingSourceAsUpdatePreservesInstanceId()
{
    const QString firstPath = m_settings.filePath(QStringLiteral("edit-source-v1.csv"));
    QVERIFY(writeFile(
        firstPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903010, .hip = 903010, .properName = "Edit Star V1", .mag = "1.0"}
        )
    ));
    const QString secondPath = m_settings.filePath(QStringLiteral("edit-source-v2.csv"));
    QVERIFY(writeFile(
        secondPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903011, .hip = 903011, .properName = "Edit Star V2", .mag = "2.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(
            QUrl::fromLocalFile(firstPath).toString(), QStringLiteral("v1")
        );
    source.title = QStringLiteral("Original Title");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString instanceId = source.instanceId;
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    // Editing title, URL, and version updates the same configured instance
    // instead of adding another one with a new ID.
    source.title = QStringLiteral("Updated Title");
    source.urls = QStringList{QUrl::fromLocalFile(secondPath).toString()};
    source.version = QStringLiteral("v2");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V2")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& candidate) {
            return candidate.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QCOMPARE(entry->title, QStringLiteral("Updated Title"));
    QCOMPARE(entry->version, QStringLiteral("v2"));

    const std::optional<SkySettingsStore::CatalogCollectionCacheSnapshot> snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->sources.size(), 2);
    QCOMPARE(snapshot->sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot->sources[0].bundled);
    QCOMPARE(snapshot->sources[1].instanceId, instanceId);
    QCOMPARE(snapshot->sources[1].version, QStringLiteral("v2"));
}

void SkyCatalogManagerTests::restoresLegacyPersistedInstanceIdsWithReferences()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));

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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Legacy Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));

    // Restart keeps the same legacy ID.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({legacyInstanceId}));
}

void SkyCatalogManagerTests::legacyMigrationRenamesDuplicateInstanceIds()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Slot Star")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 995")));

    // The renamed IDs stay stable through the migration persist and restart.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), restoredIds);
}

void SkyCatalogManagerTests::restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("archive-restore-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString archiveUrl = QStringLiteral("https://example.test/members.zip");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = archiveMemberZip(
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
    source.archiveSelector = QString::fromLatin1(kArchiveDeepSkyMember);
    source.attribution = QStringLiteral("Demo archive attribution");

    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsId(manager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(manager.starCatalog(), QStringLiteral("hip_900101")));

    // The parse contract and descriptor metadata are persisted with the source.
    // The default bundled source is persisted as configuration without a
    // payload so its position survives the restart.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->sources.size(), 2);
    const auto persistedSource = std::find_if(
        persisted->sources.begin(),
        persisted->sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(persistedSource != persisted->sources.end());
    QCOMPARE(persistedSource->descriptorId, QString("archive_demo"));
    QCOMPARE(persistedSource->version, QString("v2026.1"));
    QCOMPARE(persistedSource->archiveSelector, QString::fromLatin1(kArchiveDeepSkyMember));
    QCOMPARE(persistedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(persistedSource->attribution, QString("Demo archive attribution"));

    // Losing the binary sidecar must not lose the selected archive member.
    const QDir cacheDir(cacheDirectory);
    const QStringList binaryFiles =
        cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*.bin")}, QDir::Files);
    QCOMPARE(binaryFiles.size(), 1);
    QVERIFY(QFile::remove(cacheDir.filePath(binaryFiles.first())));

    SkyCatalogManager restoredManager(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    QVERIFY(catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));

    // The raw-payload fallback rewrote the upgraded record with the same parse
    // contract, and a reload keeps replaying the restored selection.
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.has_value());
    QCOMPARE(upgraded->sources.size(), 2);
    const auto upgradedSource = std::find_if(
        upgraded->sources.begin(),
        upgraded->sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(upgradedSource != upgraded->sources.end());
    QCOMPARE(upgradedSource->archiveSelector, QString::fromLatin1(kArchiveDeepSkyMember));
    QCOMPARE(upgradedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(upgradedSource->attribution, QString("Demo archive attribution"));

    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = archiveMemberZip(
             {.name = "NGC0993", .messier = "", .ngc = "0993", .identifiers = "PGC 9993", .commonName = "Reloaded"}
         )}
    );
    restoredManager.retrySource(instanceId);
    QTRY_VERIFY(!restoredManager.downloadingCatalog());
    QVERIFY(catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_993")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));
}

void SkyCatalogManagerTests::legacyBundledStarSlotSurvivesMigrationThroughManager()
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

void SkyCatalogManagerTests::persistsBundledSourceConfigurationWithoutNetworkOperation()
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
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->sources.size(), 1);
    QCOMPARE(snapshot->sources[0].instanceId, bundledId);
    QVERIFY(snapshot->sources[0].bundled);
    QCOMPARE(snapshot->sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!snapshot->sources[0].enabled);
    QVERIFY(snapshot->sources[0].urls.isEmpty());
    QVERIFY(snapshot->sources[0].payload.isEmpty());
    QVERIFY(snapshot->sources[0].binaryPayload.isEmpty());

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

void SkyCatalogManagerTests::restoredBundledDeepSkySourceKeepsFreshObjectCount()
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

void SkyCatalogManagerTests::interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star A")));

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
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star A")));
}

void SkyCatalogManagerTests::unreadablePayloadKeepsConfiguredSourceWithoutErasingSiblings()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Healthy Star")));

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
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->sources.size(), 3);
    const auto persistedDamaged = std::find_if(
        persisted->sources.begin(),
        persisted->sources.end(),
        [](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == QStringLiteral("custom:damaged");
        }
    );
    QVERIFY(persistedDamaged != persisted->sources.end());
    QCOMPARE(persistedDamaged->urls, QStringList{damagedUrl});
    QCOMPARE(persistedDamaged->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(persistedDamaged->enabled);
    QCOMPARE(persistedDamaged->payload, QByteArray("this is not a catalog"));

    // The preserved configuration is enough to retry the configured download.
    manager.retrySource(QStringLiteral("custom:damaged"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Recovered Star")));

    // A further restart keeps all three configured sources.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Recovered Star")));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Healthy Star")));
}

void SkyCatalogManagerTests::rejectedRestoreKeepsPreviousCollectionAndReportsError()
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
    QVERIFY(persisted.has_value());
    QVERIFY(collectionContainsInstanceId(*persisted, QStringLiteral("bundled-core")));
    QVERIFY(!collectionContainsInstanceId(*persisted, QStringLiteral("primary")));
}

void SkyCatalogManagerTests::relatedConstellationDatasetsStayOwnedByTheirSources()
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
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // Disabling one source removes only its owned dataset from the active
    // view, including its anchors and its count contribution.
    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

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
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.resolvedConstellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.resolvedConstellationAnchorGroups().size(), std::size_t{1});
    QVERIFY(manager.isSourceEnabled(sourceBId));
}

void SkyCatalogManagerTests::overlappingConstellationDatasetsFollowSourceOrder()
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
    const auto* orionGroup = findConstellationAnchorGroup(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_24436"));
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Reordering the collection changes both the winning anchors and the
    // composed declared count.
    manager.moveSource(sourceAId, 2);
    orionGroup = findConstellationAnchorGroup(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_25336"));
    QCOMPARE(manager.constellationCount(), std::size_t{3});
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{6});
}

void SkyCatalogManagerTests::lateRelatedResponseAfterOwnerRemovalIsIgnored()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);

    manager.removeSource(sourceAId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceAId));
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

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
    QVERIFY(persistedAfterRemoval.has_value());
    QVERIFY(!collectionContainsInstanceId(*persistedAfterRemoval, sourceAId));

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The removed owner's delayed response is discarded: no active lines or
    // counts, no catalog change, no status change, and no persisted owner data.
    // The retained sibling keeps its own dataset and status.
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
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
    QVERIFY(persistedAfterReply.has_value());
    QCOMPARE(persistedAfterReply->sources.size(), persistedAfterRemoval->sources.size());
    QVERIFY(!collectionContainsInstanceId(*persistedAfterReply, sourceAId));
}

void SkyCatalogManagerTests::readdedOwnerRejectsPreviousIncarnationResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/readded-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/readded-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = lyraRelatedDatasetPayload(), .manualFinish = true});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance first = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = first.instanceId;
    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> staleReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!staleReply.isNull());
    QVERIFY(!staleReply->isFinished());

    // Replace the owner with a new incarnation of the same durable instance
    // ID; only this incarnation's revision may accept the related response.
    manager.removeSource(sourceId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceId));

    skygate::ui::internal::SkyCatalogSourceInstance second = relatedDatasetInstance(catalogUrl, relatedUrl);
    second.instanceId = sourceId;
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    const auto revisionAfterCurrent = manager.catalogRevision();
    const int catalogChangesAfterCurrent = catalogSpy.count();
    const QString statusAfterCurrent = manager.statusText();
    const int statusChangesAfterCurrent = statusSpy.count();

    const auto persistedAfterCurrent = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterCurrent.has_value());
    const auto currentRecord = std::find_if(
        persistedAfterCurrent->sources.begin(),
        persistedAfterCurrent->sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(currentRecord != persistedAfterCurrent->sources.end());
    const QByteArray persistedLineRows = currentRecord->constellationLineRows;
    QVERIFY(!persistedLineRows.isEmpty());

    QVERIFY(!staleReply.isNull());
    staleReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The previous incarnation's delayed response cannot replace the current
    // owner's dataset, its counts, its status, or the persisted collection.
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(manager.catalogRevision(), revisionAfterCurrent);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterCurrent);
    QCOMPARE(manager.statusText(), statusAfterCurrent);
    QCOMPARE(statusSpy.count(), statusChangesAfterCurrent);

    const auto persistedAfterStale = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterStale.has_value());
    const auto staleRecord = std::find_if(
        persistedAfterStale->sources.begin(),
        persistedAfterStale->sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(staleRecord != persistedAfterStale->sources.end());
    QCOMPARE(staleRecord->constellationLineRows, persistedLineRows);
}

void SkyCatalogManagerTests::disablingOwnerSupersedesItsPendingRelatedResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/disabled-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/disabled-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
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

void SkyCatalogManagerTests::lateRelatedFailureStatusFromSupersededOwnerIsIgnored()
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

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
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

void SkyCatalogManagerTests::outOfOrderRelatedRepliesPopulateTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/out-of-order-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/out-of-order-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/out-of-order-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/out-of-order-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> firstReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!firstReply.isNull());
    QVERIFY(!firstReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Source B's later request completes first; loading it did not discard
    // source A's pending response.
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    // Source A's earlier request completes after, into A's own record.
    QVERIFY(!firstReply.isNull());
    firstReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Each dataset stays with the owner whose response delivered it.
    manager.disableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    manager.enableSource(sourceAId);
    manager.disableSource(sourceBId);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerTests::reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/pending-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/pending-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/pending-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/pending-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

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
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::relatedDatasetsRoundTripToTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/roundtrip-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/roundtrip-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/roundtrip-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/roundtrip-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.has_value());
    const SkySettingsStore::CatalogSourceCacheRecord* recordA = findCollectionRecord(*snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB = findCollectionRecord(*snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);

    // Each record carries the dataset its own owner downloaded; the composed
    // collection-wide view is not copied into both records.
    const auto recordALines = relatedLineRefs(*recordA);
    const auto recordBLines = relatedLineRefs(*recordB);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QCOMPARE(recordBLines.size(), std::size_t{2});
    QVERIFY(relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!relatedLineRefsContainHip(recordALines, "hip_26311"));
    QVERIFY(relatedLineRefsContainHip(recordBLines, "hip_26311"));
    QVERIFY(!relatedLineRefsContainHip(recordBLines, "hip_27989"));

    // The restart restores each dataset to its own owner.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(restoredManager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});

    restoredManager.disableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    restoredManager.enableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
}

void SkyCatalogManagerTests::disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/disabled-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/disabled-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/disabled-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/disabled-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.has_value());
    const SkySettingsStore::CatalogSourceCacheRecord* recordA = findCollectionRecord(*snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB = findCollectionRecord(*snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);
    QVERIFY(!recordA->enabled);

    // The disabled owner keeps its own inactive dataset; the persist does not
    // replace it with the composed view of the enabled sibling.
    const auto recordALines = relatedLineRefs(*recordA);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QVERIFY(relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!relatedLineRefsContainHip(recordALines, "hip_26311"));
    QCOMPARE(recordA->constellationCount, std::size_t{1});

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(!restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));

    // The disabled owner's references stay inactive after the restart.
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    // Re-enabling restores the owner's own dataset, not the sibling's.
    restoredManager.enableSource(sourceAId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    const skygate::ephemeris::ConstellationAnchorGroup* orionGroup =
        findConstellationAnchorGroup(restoredManager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.size(), std::size_t{3});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});
}

void SkyCatalogManagerTests::removedOwnerRelatedDataDoesNotReturnAfterRestart()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // A first restart restores both owners from their own records.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);

    // Removing one owner removes its dataset with its record.
    restoredManager.removeSource(sourceBId);
    QVERIFY(!restoredManager.sourceInstanceIds().contains(sourceBId));
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    const auto afterRemoval = store.loadCatalogCollectionCache();
    QVERIFY(afterRemoval.has_value());
    QVERIFY(!collectionContainsInstanceId(*afterRemoval, sourceBId));
    const SkySettingsStore::CatalogSourceCacheRecord* survivingRecord = findCollectionRecord(*afterRemoval, sourceAId);
    QVERIFY(survivingRecord != nullptr);
    const auto survivingLines = relatedLineRefs(*survivingRecord);
    QVERIFY(!relatedLineRefsContainHip(survivingLines, "hip_26311"));

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
    QVERIFY(findConstellationAnchorGroup(secondRestart, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(secondRestart, "Lyra") == nullptr);
    QCOMPARE(secondRestart.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/corrupt-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/corrupt-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/corrupt-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/corrupt-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // Corrupt one owner's stored related payload in place.
    auto corruptedSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(corruptedSnapshot.has_value());
    const auto corruptedRecord = std::find_if(
        corruptedSnapshot->sources.begin(),
        corruptedSnapshot->sources.end(),
        [&sourceAId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == sourceAId;
        }
    );
    QVERIFY(corruptedRecord != corruptedSnapshot->sources.end());
    corruptedRecord->constellationLineRows = "not a related line payload";
    QVERIFY(store.saveCatalogCollectionCache(*corruptedSnapshot));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Saved related constellation dataset is unreadable"));
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());

    // The corrupted owner restores without related data and keeps its
    // configuration; the sibling's dataset is untouched.
    QVERIFY(restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    restoredManager.disableSource(sourceBId);
    QVERIFY(restoredManager.constellationLineRefs().empty());
    restoredManager.enableSource(sourceBId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerTests::migratesPriorSingleOwnerRelatedPayloadOnce()
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
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // The migration rewrite commits the record in the per-source format and
    // keeps the adopted dataset under its owner.
    const auto migrated = store.loadCatalogCollectionCache();
    QVERIFY(migrated.has_value());
    QCOMPARE(
        migrated->schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    const SkySettingsStore::CatalogSourceCacheRecord* migratedRecord =
        findCollectionRecord(*migrated, QStringLiteral("custom:prior-owner"));
    QVERIFY(migratedRecord != nullptr);
    QCOMPARE(relatedLineRefs(*migratedRecord).size(), std::size_t{2});

    // The next start reads the migrated records without another migration and
    // with the owner's dataset intact.
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restarted, "Orion") != nullptr);
    QCOMPARE(restarted.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::removedBundledSourceDoesNotReturnAfterRestart()
{
    const QString siblingPath = m_settings.filePath(QStringLiteral("removed-bundled-sibling.csv"));
    QVERIFY(writeFile(
        siblingPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 906020, .hip = 906020, .properName = "Sibling Star", .mag = "1.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    manager.addSourceUrl(QUrl::fromLocalFile(siblingPath).toString(), QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString siblingId = manager.sourceInstanceIds().last();

    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.sourceInstanceIds(), QStringList{siblingId});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->sources.size(), 1);
    QCOMPARE(snapshot->sources[0].instanceId, siblingId);

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{siblingId});
    const auto composedIds = restoredManager.sourceIds();
    QVERIFY(std::none_of(composedIds.begin(), composedIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("primary");
    }));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Sibling Star")));
}

void SkyCatalogManagerTests::presentationSummarizesEnabledCollectionParticipation()
{
    const QString starAPath = m_settings.filePath(QStringLiteral("presentation-star-a.csv"));
    const QString starBPath = m_settings.filePath(QStringLiteral("presentation-star-b.csv"));
    const QString deepSkyPath = m_settings.filePath(QStringLiteral("presentation-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900701, .hip = 900701, .properName = "Presentation Star A", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900702, .hip = 900702, .properName = "Presentation Star B", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        skygate::ui::tests::sampleOpenNgcCsvPayload(
            {.name = "NGC0702",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0702",
             .identifiers = "PGC 7002",
             .commonName = "Presentation Galaxy"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    // The bundled star catalog already supplies the bundled deep-sky objects,
    // so the enabled fallback contributes nothing and is not named.
    QVERIFY(manager.participationSummary().startsWith(QStringLiteral("Bundled")));
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    // Three mixed configured sources: two star sources and one deep-sky source.
    skygate::ui::internal::SkyCatalogSourceInstance starA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starAPath).toString());
    starA.title = QStringLiteral("Alpha");
    skygate::ui::internal::SkyCatalogSourceInstance starB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starBPath).toString());
    starB.title = QStringLiteral("Beta");
    skygate::ui::internal::SkyCatalogSourceInstance deepSky =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(deepSkyPath).toString());
    deepSky.title = QStringLiteral("Gamma");

    manager.loadSource(starA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(starB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(deepSky, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The summary names every enabled source in visible collection order, and
    // the status text pairs it with the active snapshot counts.
    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: %1 (").arg(manager.participationSummary())));
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().contains(
        QStringLiteral("(%1 objects,").arg(locale.toString(static_cast<qulonglong>(manager.bodyCount())))
    ));

    // Disabling one source removes it from the summary and keeps its own state
    // row, so a configured but disabled source is never presented as active.
    const int statusChangesBeforeDisable = statusSpy.count();
    manager.disableSource(starB.instanceId);
    QVERIFY(statusSpy.count() > statusChangesBeforeDisable);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Gamma"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Beta")));

    const auto findViewEntry = [](const QVector<SkyCatalogManager::SourceViewEntry>& entries,
                                  const QString& instanceId) {
        return std::find_if(
            entries.begin(), entries.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
                return entry.instanceId == instanceId;
            }
        );
    };
    const QVector<SkyCatalogManager::SourceViewEntry> disabledView = manager.sourceViewEntries();
    QCOMPARE(disabledView.size(), 4);
    const auto disabledEntry = findViewEntry(disabledView, starB.instanceId);
    QVERIFY(disabledEntry != disabledView.end());
    QVERIFY(!disabledEntry->enabled);
    QCOMPARE(disabledEntry->statusText, QStringLiteral("Disabled"));
    const auto enabledEntry = findViewEntry(disabledView, starA.instanceId);
    QVERIFY(enabledEntry != disabledView.end());
    QVERIFY(enabledEntry->enabled);
    QCOMPARE(enabledEntry->statusText, QStringLiteral("Active"));
    const auto deepSkyEntry = findViewEntry(disabledView, deepSky.instanceId);
    QVERIFY(deepSkyEntry != disabledView.end());
    QCOMPARE(deepSkyEntry->title, QStringLiteral("Gamma"));
    QCOMPARE(deepSkyEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // Re-enabling restores the source in its collection position.
    manager.enableSource(starB.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));

    // Reordering the collection reorders the summary.
    manager.moveSource(deepSky.instanceId, 1);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Alpha + Beta"));

    // Removing a source drops its row, its title, and its objects.
    manager.removeSource(starA.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Beta"));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Presentation Star A")));
    QCOMPARE(manager.sourceViewEntries().size(), 3);
}

void SkyCatalogManagerTests::bundledFallbackPresentationFollowsParticipationAndRestart()
{
    const QString starsPath = m_settings.filePath(QStringLiteral("fallback-stars.csv"));
    QVERIFY(writeFile(
        starsPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900703, .hip = 900703, .properName = "Fallback Star", .mag = "1.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    // The bundled star source supplies the bundled deep-sky objects, so the
    // enabled fallback is not presented as the active deep-sky source.
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    skygate::ui::internal::SkyCatalogSourceInstance stars =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starsPath).toString());
    stars.title = QStringLiteral("Stars");
    manager.loadSource(stars, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Without the bundled star source the explicit fallback supplies the
    // deep-sky identities and the summary names it through its provenance.
    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core + Bundled Messier"));
    QVERIFY(manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // Turning the fallback participation off drops its name and its objects.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.retrySource(stars.instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // A restart restores the configured collection, the source titles, and the
    // fallback participation of the restarted configuration.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds().size(), 1);
    QVERIFY(restoredManager.participationSummary().startsWith(QStringLiteral("Stars (saved)")));
    QVERIFY(restoredManager.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(restoredManager.statusText().startsWith(QStringLiteral("Catalog: Stars (saved)")));
}

QTEST_GUILESS_MAIN(SkyCatalogManagerTests)

#include "SkyCatalogManagerTests.moc"
