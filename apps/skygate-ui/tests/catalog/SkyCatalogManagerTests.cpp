#include "CatalogCacheTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkySettingsStore.hpp"

#include <QFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>

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

constexpr int kStaleConstellationDelayMs = 500;

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
    QCOMPARE(cacheAfterClear->sources.size(), 1);
    QCOMPARE(cacheAfterClear->sources[0].instanceId, bId);
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

    // Restart restores both instances with their durable IDs and order.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({secondId, firstId}));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance removes independently.
    restoredManager.removeSource(firstId);
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({secondId}));
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
    QCOMPARE(snapshot->sources.size(), 2);
    QCOMPARE(snapshot->sources[0].instanceId, first.instanceId);
    QCOMPARE(snapshot->sources[0].version, QStringLiteral("v1"));
    QCOMPARE(snapshot->sources[0].archiveSelector, QStringLiteral("members/catalog-v1.csv"));
    QCOMPARE(snapshot->sources[1].instanceId, second.instanceId);
    QCOMPARE(snapshot->sources[1].version, QStringLiteral("v2"));
    QCOMPARE(snapshot->sources[1].archiveSelector, QStringLiteral("members/catalog-v2.csv"));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({first.instanceId, second.instanceId}));
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
    QCOMPARE(snapshot->sources.size(), 1);
    QCOMPARE(snapshot->sources[0].instanceId, instanceId);
    QCOMPARE(snapshot->sources[0].version, QStringLiteral("v2"));
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

QTEST_GUILESS_MAIN(SkyCatalogManagerTests)

#include "SkyCatalogManagerTests.moc"
