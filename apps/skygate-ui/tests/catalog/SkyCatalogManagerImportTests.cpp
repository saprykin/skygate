#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceInstanceTestSupport.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogArchiveFixture.hpp"
#include "support/CatalogBodyQuery.hpp"
#include "support/CatalogSourceFactsQuery.hpp"
#include "support/CatalogTestPayloadFile.hpp"
#include "support/CatalogTestPayloads.hpp"
#include "support/FakeNetworkAccessManager.hpp"
#include "support/FakeReplyQuery.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QThreadPool>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <vector>

using skygate::ui::tests::CatalogArchiveFixture;
using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::CatalogTestPayloadFile;
using skygate::ui::tests::FakeReplyQuery;

class SkyCatalogManagerImportTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void customDeepSkyDownloadUsesGenericSourceLabel();
    void localDownloadTogglesBusyAndAppliesCatalog();
    void cancelCatalogDownloadClearsBusyAndIgnoresResult();
    void cancelAbortsOnlyOwnTransfers();
    void cancelWhileParsingIgnoresQueuedCompletion();
    void cancelFromStartupNotificationLeavesNoRequest();
    void cancelFromFirstRowNotificationLeavesNoRequest();
    void retryAfterFirstRowNotificationCancelSucceeds();
    void failedLocalDownloadClearsBusyAndReportsStatus();
    void failedLoadPreservesPriorDataAndRetrySucceeds();
    void failedUpdateKeepsAcceptedFactsWhenPersisting();
    void acceptedFactsSurviveRestartAfterFailedUpdate();
    void successfulRetryInstallsUpdateFactsTogether();
    void canceledUpdateKeepsAcceptedSourceFacts();
    void parseFailedUpdateKeepsAcceptedSourceFacts();
    void rejectedParsedUpdateKeepsAcceptedSourceFacts();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerImportTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerImportTests")));
}

void SkyCatalogManagerImportTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerImportTests::customDeepSkyDownloadUsesGenericSourceLabel()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-deep-sky.csv"));
    QVERIFY(CatalogTestPayloadFile::write(catalogPath, skygate::ui::tests::sampleOpenNgcCsvPayload()));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    manager.loadSource(
        skygate::ui::tests::customSourceInstance(catalogUrl), skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly
    );
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(manager.sourceTitles().values().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.sourceTitles().values().contains(QStringLiteral("OpenNGC")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.statusText().contains(QStringLiteral("OpenNGC")));
}

void SkyCatalogManagerImportTests::localDownloadTogglesBusyAndAppliesCatalog()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-stars.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            catalogPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.hip = 900001, .properName = "Manager Downloaded Star", .mag = "1.0"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy processingSpy(&manager, &SkyCatalogManager::catalogProcessingChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadSource(
        skygate::ui::tests::customSourceInstance(QUrl::fromLocalFile(catalogPath).toString()),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QVERIFY(manager.downloadingCatalog());
    QVERIFY(!manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Cannot clear cache while download is in progress"));

    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.catalogProcessing());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QString("Manager Downloaded Star")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(downloadSpy.count() >= 2);
    QVERIFY(processingSpy.count() >= 2);
    QVERIFY(catalogSpy.count() >= 1);
    QVERIFY(statusSpy.count() >= 2);
}

void SkyCatalogManagerImportTests::cancelCatalogDownloadClearsBusyAndIgnoresResult()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-cancel-stars.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            catalogPath,
            skygate::ui::tests::sampleHygCsvPayload({
                .hip = 900002,
                .properName = "Canceled Manager Star",
                .mag = "1.0",
            })
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.loadSource(
        skygate::ui::tests::customSourceInstance(
            QUrl::fromLocalFile(catalogPath).toString(), QStringLiteral("primary")
        ),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
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
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QString("Canceled Manager Star")));
    QVERIFY(downloadSpy.count() >= 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerImportTests::cancelAbortsOnlyOwnTransfers()
{
    const QString ownedAcceptedUrl = QStringLiteral("https://example.test/scope-accepted.csv");
    const QString ownedPendingUrl = QStringLiteral("https://example.test/scope-pending.csv");
    const QString ownedFallbackUrl = QStringLiteral("https://example.test/scope-fallback.csv");
    const QString otherOwnerUrl = QStringLiteral("https://example.test/scope-other-owner.csv");
    const QByteArray pendingPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908100, .hip = 908100, .properName = "Scope Update Star", .mag = "1.0"}
    );
    const QByteArray otherOwnerPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908200, .hip = 908200, .properName = "Other Owner Star", .mag = "1.0"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        ownedAcceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908000, .hip = 908000, .properName = "Scope Accepted Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(ownedPendingUrl, {.payload = pendingPayload, .manualFinish = true});
    networkAccessManager.enqueueResponse(ownedFallbackUrl, {.payload = pendingPayload});
    networkAccessManager.enqueueResponse(otherOwnerUrl, {.payload = otherOwnerPayload, .manualFinish = true});

    SkySettingsStore ownedStore;
    SkySettingsStore otherStore;
    SkyCatalogManager ownedManager(&ownedStore, nullptr, nullptr, &networkAccessManager);
    SkyCatalogManager otherManager(&otherStore, nullptr, nullptr, &networkAccessManager);
    QSignalSpy ownedCatalogSpy(&ownedManager, &SkyCatalogManager::catalogChanged);
    QSignalSpy otherCatalogSpy(&otherManager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(ownedAcceptedUrl, QStringLiteral("v1"));
    const QString instanceId = accepted.instanceId;
    ownedManager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!ownedManager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(ownedManager.starCatalog(), QStringLiteral("Scope Accepted Star")));

    // The pending update carries a fallback candidate a cancellation must never
    // request, and the other owner shares the same network manager.
    skygate::ui::internal::SkyCatalogSourceInstance update = accepted;
    update.title = QStringLiteral("Scope Update Title");
    update.version = QStringLiteral("v2");
    update.urls = QStringList{ownedPendingUrl, ownedFallbackUrl};
    ownedManager.loadSource(update, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    otherManager.loadSource(
        skygate::ui::tests::customSourceInstance(otherOwnerUrl), skygate::ephemeris::CatalogCompositionPolicy::Merge
    );

    QVERIFY(ownedManager.downloadingCatalog());
    QVERIFY(otherManager.downloadingCatalog());
    skygate::ui::tests::FakeNetworkReply* ownedReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, ownedPendingUrl);
    skygate::ui::tests::FakeNetworkReply* otherReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, otherOwnerUrl);
    QVERIFY(ownedReply != nullptr);
    QVERIFY(otherReply != nullptr);

    const auto revisionBeforeCancel = ownedManager.catalogRevision();
    const std::size_t bodyCountBeforeCancel = ownedManager.starCatalog()->bodies().size();
    const int changesBeforeCancel = ownedCatalogSpy.count();

    ownedManager.cancelCatalogDownload();

    QVERIFY(ownedReply->isAborted());
    QVERIFY(!otherReply->isAborted());
    QVERIFY(!otherReply->isFinished());
    QVERIFY(!ownedManager.downloadingCatalog());
    QVERIFY(otherManager.downloadingCatalog());
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({ownedAcceptedUrl, ownedPendingUrl, otherOwnerUrl}));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({ownedPendingUrl}));
    QCOMPARE(ownedManager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(ownedManager.starCatalog()->bodies().size(), bodyCountBeforeCancel);
    QCOMPARE(ownedCatalogSpy.count(), changesBeforeCancel);
    QVERIFY(CatalogBodyQuery::containsDisplayName(ownedManager.starCatalog(), QStringLiteral("Scope Accepted Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(ownedManager.starCatalog(), QStringLiteral("Scope Update Star")));

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = ownedManager.sourceViewEntries();
    const auto canceledEntry = std::find_if(
        canceledView.begin(), canceledView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(canceledEntry != canceledView.end());
    QVERIFY(!canceledEntry->busy);
    QCOMPARE(canceledEntry->statusText, QStringLiteral("Canceled"));

    // The other owner's transfer completes normally on the shared manager.
    otherReply->finishNow();
    QTRY_VERIFY(!otherManager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(otherManager.starCatalog(), QStringLiteral("Other Owner Star")));
    QVERIFY(otherCatalogSpy.count() >= 1);
    QVERIFY(!CatalogBodyQuery::containsDisplayName(ownedManager.starCatalog(), QStringLiteral("Other Owner Star")));
}

void SkyCatalogManagerImportTests::cancelWhileParsingIgnoresQueuedCompletion()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/parse-cancel-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/parse-cancel-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908300, .hip = 908300, .properName = "Before Cancel Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    const QString instanceId = accepted.instanceId;
    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Before Cancel Star")));

    const auto revisionBeforeCancel = manager.catalogRevision();
    const std::size_t bodyCountBeforeCancel = manager.starCatalog()->bodies().size();
    const skygate::ephemeris::IStarCatalog* const catalogBeforeCancel = manager.starCatalog();
    const QString acceptedTitleBeforeCancel = manager.sourceTitles().value(instanceId);
    const int changesBeforeCancel = catalogSpy.count();

    // The reply completes and hands its payload to the background parser; the
    // parsed result is still queued when the operation is canceled.
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908301, .hip = 908301, .properName = "After Cancel Star", .mag = "2.0"}
         ),
         .manualFinish = true}
    );
    skygate::ui::internal::SkyCatalogSourceInstance update = accepted;
    update.title = QStringLiteral("Parse Cancel Update Title");
    update.version = QStringLiteral("v2");
    update.urls = QStringList{updateUrl};
    manager.loadSource(update, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(manager.downloadingCatalog());

    skygate::ui::tests::FakeNetworkReply* reply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, updateUrl);
    QVERIFY(reply != nullptr);
    reply->finishNow();
    QVERIFY(manager.catalogProcessing());

    manager.cancelCatalogDownload();
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));

    // The queued parse result finds a superseded operation and is discarded.
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();

    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(manager.starCatalog()->bodies().size(), bodyCountBeforeCancel);
    QCOMPARE(manager.starCatalog(), catalogBeforeCancel);
    QCOMPARE(manager.sourceTitles().value(instanceId), acceptedTitleBeforeCancel);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Before Cancel Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("After Cancel Star")));
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = manager.sourceViewEntries();
    const auto canceledEntry = std::find_if(
        canceledView.begin(), canceledView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(canceledEntry != canceledView.end());
    QVERIFY(!canceledEntry->busy);
    QCOMPARE(canceledEntry->statusText, QStringLiteral("Canceled"));
}

void SkyCatalogManagerImportTests::cancelFromStartupNotificationLeavesNoRequest()
{
    const QString catalogUrl = QStringLiteral("https://example.test/startup-cancel.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    // A request that started despite the cancellation would stay open, so the
    // test observes a leak instead of racing an automatic completion.
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 909000, .hip = 909000, .properName = "Startup Cancel Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const auto acceptedRevisionBefore = manager.catalogRevision();
    const std::size_t acceptedBodiesBefore = manager.starCatalog()->bodies().size();
    const skygate::ephemeris::IStarCatalog* const acceptedCatalogBefore = manager.starCatalog();

    bool canceledFromStartup = false;
    QObject::connect(
        &manager, &SkyCatalogManager::downloadingCatalogChanged, &manager, [&manager, &canceledFromStartup]() {
            if (manager.downloadingCatalog() && !canceledFromStartup) {
                canceledFromStartup = true;
                manager.cancelCatalogDownload();
            }
        }
    );

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(canceledFromStartup);
    QCoreApplication::processEvents();

    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(networkAccessManager.issuedReplies().isEmpty());
    QVERIFY(networkAccessManager.requestedUrls().isEmpty());
    QCOMPARE(manager.catalogRevision(), acceptedRevisionBefore);
    QCOMPARE(manager.starCatalog()->bodies().size(), acceptedBodiesBefore);
    QCOMPARE(manager.starCatalog(), acceptedCatalogBefore);
    QCOMPARE(catalogSpy.count(), 0);
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& viewEntry) {
            return viewEntry.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QVERIFY(!entry->busy);
    QVERIFY(!entry->hasError);
    QCOMPARE(entry->statusText, QStringLiteral("Canceled"));
}

void SkyCatalogManagerImportTests::cancelFromFirstRowNotificationLeavesNoRequest()
{
    const QString catalogUrl = QStringLiteral("https://example.test/first-row-cancel.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    // A request that started despite the cancellation would stay open, so the
    // test observes a leak instead of racing an automatic completion.
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 909100, .hip = 909100, .properName = "First Row Cancel Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const auto acceptedRevisionBefore = manager.catalogRevision();
    const std::size_t acceptedBodiesBefore = manager.starCatalog()->bodies().size();
    const skygate::ephemeris::IStarCatalog* const acceptedCatalogBefore = manager.starCatalog();

    bool canceledFromFirstRow = false;
    bool downloadingWhenNotified = true;
    QObject::connect(
        &manager,
        &SkyCatalogManager::sourcesChanged,
        &manager,
        [&manager, &canceledFromFirstRow, &downloadingWhenNotified]() {
            if (canceledFromFirstRow) {
                return;
            }
            canceledFromFirstRow = true;
            // The busy row is published before the download starts, so the
            // first notification arrives with no transfer running yet.
            downloadingWhenNotified = manager.downloadingCatalog();
            manager.cancelCatalogDownload();
        }
    );

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(canceledFromFirstRow);
    QVERIFY(!downloadingWhenNotified);
    QCoreApplication::processEvents();

    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(networkAccessManager.issuedReplies().isEmpty());
    QVERIFY(networkAccessManager.requestedUrls().isEmpty());
    QCOMPARE(manager.catalogRevision(), acceptedRevisionBefore);
    QCOMPARE(manager.starCatalog()->bodies().size(), acceptedBodiesBefore);
    QCOMPARE(manager.starCatalog(), acceptedCatalogBefore);
    QCOMPARE(catalogSpy.count(), 0);
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& viewEntry) {
            return viewEntry.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QVERIFY(!entry->busy);
    QVERIFY(!entry->hasError);
    QCOMPARE(entry->statusText, QStringLiteral("Canceled"));
}

void SkyCatalogManagerImportTests::retryAfterFirstRowNotificationCancelSucceeds()
{
    const QString catalogUrl = QStringLiteral("https://example.test/first-row-retry.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 909200, .hip = 909200, .properName = "First Row Retry Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    bool canceledFromFirstRow = false;
    QObject::connect(&manager, &SkyCatalogManager::sourcesChanged, &manager, [&manager, &canceledFromFirstRow]() {
        if (canceledFromFirstRow) {
            return;
        }
        canceledFromFirstRow = true;
        manager.cancelCatalogDownload();
    });

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCoreApplication::processEvents();

    QVERIFY(canceledFromFirstRow);
    // The canceled startup issued no request, so the queued response is still
    // available to the retry.
    QVERIFY(networkAccessManager.requestedUrls().isEmpty());
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());

    manager.retrySource(instanceId);
    QVERIFY(manager.downloadingCatalog());
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("First Row Retry Star")));
    QVERIFY(!manager.statusText().contains(QStringLiteral("canceled"), Qt::CaseInsensitive));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& viewEntry) {
            return viewEntry.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QVERIFY(!entry->busy);
    QVERIFY(!entry->hasError);
    QCOMPARE(entry->statusText, QStringLiteral("Active"));
}

void SkyCatalogManagerImportTests::failedLocalDownloadClearsBusyAndReportsStatus()
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
    manager.loadSource(
        skygate::ui::tests::customSourceInstance(missingCatalogUrl), skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QVERIFY(manager.downloadingCatalog());

    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(manager.statusText().contains(QStringLiteral("failed"), Qt::CaseInsensitive));
    QVERIFY(manager.statusText().contains(missingCatalogUrl));
    QCOMPARE(manager.catalogRevision(), originalRevision);
    QCOMPARE(downloadSpy.count(), 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerImportTests::failedLoadPreservesPriorDataAndRetrySucceeds()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString sourceAPath = m_settings.filePath(QStringLiteral("retry-source-a.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            sourceAPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 901010, .hip = 901010, .properName = "Retry Source A", .mag = "1.0"}
            )
        )
    );
    const QString sourceAUrl = QUrl::fromLocalFile(sourceAPath).toString();

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    loadLocalSource(sourceAUrl);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    const std::uint64_t revisionAfterA = manager.catalogRevision();

    const QString retryPath = m_settings.filePath(QStringLiteral("retry-source-b.csv"));
    const QString retryUrl = QUrl::fromLocalFile(retryPath).toString();

    // The published snapshot, its provenance, and its presentation stay
    // untouched by a failed publication: only the operation reports the error.
    const skygate::ephemeris::IStarCatalog* const catalogBeforeFailure = manager.starCatalog();
    const std::size_t bodyCountBeforeFailure = manager.starCatalog()->bodies().size();
    const QStringList sourceIdsBeforeFailure(manager.sourceIds().begin(), manager.sourceIds().end());
    const std::vector<QStringList> contributorsBeforeFailure = manager.contributorSourceIds();
    const QString summaryBeforeFailure = manager.participationSummary();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

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
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.starCatalog(), catalogBeforeFailure);
    QCOMPARE(manager.starCatalog()->bodies().size(), bodyCountBeforeFailure);
    QCOMPARE(QStringList(manager.sourceIds().begin(), manager.sourceIds().end()), sourceIdsBeforeFailure);
    QCOMPARE(manager.contributorSourceIds().size(), contributorsBeforeFailure.size());
    for (std::size_t index = 0; index < contributorsBeforeFailure.size(); ++index) {
        QCOMPARE(manager.contributorSourceIds()[index], contributorsBeforeFailure[index]);
    }
    QCOMPARE(manager.participationSummary(), summaryBeforeFailure);
    QCOMPARE(catalogSpy.count(), 0);
    QVERIFY(manager.statusText().contains(QStringLiteral("failed")));

    QVERIFY(
        CatalogTestPayloadFile::write(
            retryPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 901011, .hip = 901011, .properName = "Retry Source B", .mag = "1.0"}
            )
        )
    );
    manager.retrySource(retrySourceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source B")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{3});
}

// An update attempt that never becomes active must not become the source's
// configurable facts: the still-active source keeps its accepted descriptor,
// parse contract, and payload, and ordinary persistence serializes only those.
void SkyCatalogManagerImportTests::failedUpdateKeepsAcceptedFactsWhenPersisting()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/accepted-archive.zip");
    const QString rejectedUrl = QStringLiteral("https://example.test/rejected-archive.zip");
    const QString unreachableUrl = QStringLiteral("https://example.test/unreachable-archive.zip");
    const QByteArray acceptedPayload = CatalogArchiveFixture::archiveMemberZip(
        {.name = "NGC0991", .messier = "", .ngc = "0991", .identifiers = "PGC 9991", .commonName = "Accepted Member"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Accepted V1 Title");
    accepted.descriptorId = QStringLiteral("accepted_descriptor");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    accepted.archiveSelector = CatalogArchiveFixture::starsMemberPath();
    accepted.attribution = QStringLiteral("Accepted attribution");
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));
    QVERIFY(!CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("ngc_991")));

    // The same instance is asked to update itself to a different archive
    // member, schema hint, version, title, attribution, and related dataset.
    // The download fails, so none of it may be remembered as accepted.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Rejected V2 Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{rejectedUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    rejected.archiveSelector = CatalogArchiveFixture::deepSkyMemberPath();
    rejected.attribution = QStringLiteral("Rejected attribution");
    rejected.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/rejected-lines.json")};

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/rejected-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(!failedEntry->busy);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("failed")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Accepted V1 Title"));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));

    // Reordering the still-active source persists the accepted facts alone.
    manager.moveSource(instanceId, 0);
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Accepted V1 Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->descriptorId, QStringLiteral("accepted_descriptor"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(record->archiveSelector, CatalogArchiveFixture::starsMemberPath());
    QCOMPARE(record->attribution, QStringLiteral("Accepted attribution"));
    QVERIFY(record->relatedDatasetUrls.isEmpty());
    QCOMPARE(record->payload, acceptedPayload);

    // A second attempt against a URL the network cannot reach keeps the same
    // boundary: the operation reports the failure and persistence still
    // serializes the accepted facts.
    rejected.urls = QStringList{unreachableUrl};
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/unreachable-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.statusText().contains(QStringLiteral("failed"), Qt::CaseInsensitive));

    manager.moveSource(instanceId, 1);
    const SkyCatalogCollectionCacheLoadResult persistedAfterRetry = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterRetry.isLoaded());
    const SkyCatalogSourceCacheRecord* recordAfterRetry =
        CatalogSourceFactsQuery::findRecord(persistedAfterRetry.snapshot, instanceId);
    QVERIFY(recordAfterRetry != nullptr);
    QCOMPARE(recordAfterRetry->title, QStringLiteral("Accepted V1 Title"));
    QCOMPARE(recordAfterRetry->version, QStringLiteral("v1"));
    QCOMPARE(recordAfterRetry->urls, QStringList{acceptedUrl});
    QCOMPARE(recordAfterRetry->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(recordAfterRetry->archiveSelector, CatalogArchiveFixture::starsMemberPath());
    QVERIFY(recordAfterRetry->relatedDatasetUrls.isEmpty());
    QCOMPARE(recordAfterRetry->payload, acceptedPayload);
}

void SkyCatalogManagerImportTests::acceptedFactsSurviveRestartAfterFailedUpdate()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("failed-update-restart-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString acceptedUrl = QStringLiteral("https://example.test/restart-accepted-archive.zip");
    const QString rejectedUrl = QStringLiteral("https://example.test/restart-rejected-archive.zip");
    const QByteArray acceptedPayload = CatalogArchiveFixture::archiveMemberZip(
        {.name = "NGC0992", .messier = "", .ngc = "0992", .identifiers = "PGC 9992", .commonName = "Restart Accepted"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Restart Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    accepted.archiveSelector = CatalogArchiveFixture::starsMemberPath();
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Restart Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{rejectedUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    rejected.archiveSelector = CatalogArchiveFixture::deepSkyMemberPath();

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/restart-rejected-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The reorder writes the collection snapshot the restarts read back.
    manager.moveSource(instanceId, 0);

    // A restart from the binary cache rebuilds exactly the accepted source.
    {
        SkyCatalogManager binaryRestart(&store, nullptr, nullptr, &networkAccessManager);
        QVERIFY(binaryRestart.restoreCatalogCache());
        QCOMPARE(binaryRestart.sourceInstanceIds().count(instanceId), 1);
        QVERIFY(
            CatalogBodyQuery::containsDisplayName(binaryRestart.starCatalog(), QStringLiteral("Archive Member Star"))
        );
        QVERIFY(!CatalogBodyQuery::containsId(binaryRestart.starCatalog(), QStringLiteral("ngc_992")));
        QCOMPARE(
            CatalogSourceFactsQuery::durableTitle(binaryRestart.sourceTitles().value(instanceId)),
            QStringLiteral("Restart Accepted Title")
        );
    }

    // Losing the binary sidecar must not make the raw fallback parse the
    // accepted bytes with the rejected attempt's member selection and hint.
    const QString binaryPath = CatalogSourceFactsQuery::sidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    QVERIFY(!binaryPath.isEmpty());
    QVERIFY(QFile::remove(binaryPath));

    {
        SkyCatalogManager rawRestart(&store, nullptr, nullptr, &networkAccessManager);
        QVERIFY(rawRestart.restoreCatalogCache());
        QCOMPARE(rawRestart.sourceInstanceIds().count(instanceId), 1);
        QVERIFY(CatalogBodyQuery::containsDisplayName(rawRestart.starCatalog(), QStringLiteral("Archive Member Star")));
        QVERIFY(!CatalogBodyQuery::containsId(rawRestart.starCatalog(), QStringLiteral("ngc_992")));
        QCOMPARE(
            CatalogSourceFactsQuery::durableTitle(rawRestart.sourceTitles().value(instanceId)),
            QStringLiteral("Restart Accepted Title")
        );

        const SkyCatalogCollectionCacheLoadResult upgraded = store.loadCatalogCollectionCache();
        QVERIFY(upgraded.isLoaded());
        const SkyCatalogSourceCacheRecord* upgradedRecord =
            CatalogSourceFactsQuery::findRecord(upgraded.snapshot, instanceId);
        QVERIFY(upgradedRecord != nullptr);
        QCOMPARE(upgradedRecord->version, QStringLiteral("v1"));
        QCOMPARE(upgradedRecord->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
        QCOMPARE(upgradedRecord->archiveSelector, CatalogArchiveFixture::starsMemberPath());
        QCOMPARE(upgradedRecord->payload, acceptedPayload);
    }
}

void SkyCatalogManagerImportTests::successfulRetryInstallsUpdateFactsTogether()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/retry-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/retry-update.csv");
    const QByteArray updatePayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0998", .messier = "", .ngc = "0998", .identifiers = "PGC 9998", .commonName = "Retried Galaxy"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908001, .hip = 908001, .properName = "Retry Accepted Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Retry Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Accepted Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Retried Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed https://example\\.test/retry-update\\.csv .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Persisting while the attempt is still only an attempt serializes the
    // accepted facts; the successful retry below then replaces them together.
    manager.moveSource(instanceId, 0);
    {
        const SkyCatalogCollectionCacheLoadResult persistedBeforeRetry = store.loadCatalogCollectionCache();
        QVERIFY(persistedBeforeRetry.isLoaded());
        const SkyCatalogSourceCacheRecord* recordBeforeRetry =
            CatalogSourceFactsQuery::findRecord(persistedBeforeRetry.snapshot, instanceId);
        QVERIFY(recordBeforeRetry != nullptr);
        QCOMPARE(recordBeforeRetry->urls, QStringList{acceptedUrl});
        QCOMPARE(recordBeforeRetry->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    }

    // The failed attempt stays retryable and its success commits the new
    // metadata and the data it was activated with in one step, under the same
    // durable instance ID.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("ngc_998")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Retry Accepted Star")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Retried Title"));

    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Retried Title"));
    QCOMPARE(record->version, QStringLiteral("v2"));
    QCOMPARE(record->urls, QStringList{updateUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(record->payload, updatePayload);

    SkyCatalogManager restarted(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(CatalogBodyQuery::containsId(restarted.starCatalog(), QStringLiteral("ngc_998")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Retry Accepted Star")));
    QCOMPARE(
        CatalogSourceFactsQuery::durableTitle(restarted.sourceTitles().value(instanceId)),
        QStringLiteral("Retried Title")
    );
}

void SkyCatalogManagerImportTests::canceledUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/cancel-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/cancel-update.csv");
    const QByteArray updatePayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0997", .messier = "", .ngc = "0997", .identifiers = "PGC 9997", .commonName = "Canceled Galaxy"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908010, .hip = 908010, .properName = "Cancel Accepted Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Cancel Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));

    // The update download stays open until the test cancels it.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload, .manualFinish = true});
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Canceled Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(manager.downloadingCatalog());

    manager.cancelCatalogDownload();
    QVERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = manager.sourceViewEntries();
    const auto canceledEntry = std::find_if(
        canceledView.begin(), canceledView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(canceledEntry != canceledView.end());
    QVERIFY(!canceledEntry->busy);
    QVERIFY(!canceledEntry->hasError);
    QCOMPARE(canceledEntry->statusText, QStringLiteral("Canceled"));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));

    // Persisting after the cancellation still serializes the accepted facts.
    manager.moveSource(instanceId, 0);
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Cancel Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);

    // The canceled attempt is still the configuration a retry repeats.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("ngc_997")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Canceled Title"));
}

void SkyCatalogManagerImportTests::parseFailedUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/parse-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/parse-failed-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908020, .hip = 908020, .properName = "Parse Accepted Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(updateUrl, {.payload = "not a catalog payload\n"});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Parse Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Parse Accepted Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Parse Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog: Source https://example\\.test/parse-failed-update\\.csv parse failed")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("parse failed")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Parse Accepted Star")));

    manager.moveSource(instanceId, 0);
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Parse Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(
        record->payload,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 908020, .hip = 908020, .properName = "Parse Accepted Star", .mag = "1.0"}
        )
    );
}

void SkyCatalogManagerImportTests::rejectedParsedUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/composition-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/composition-update.csv");
    const QByteArray acceptedPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908030, .hip = 908030, .properName = "Composition Accepted Star", .mag = "1.0"}
    );
    const QByteArray updatePayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908031, .hip = 908031, .properName = "Composition Parsed Star", .mag = "2.0"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    // The bundled deep-sky fallback is disabled while the source loads, so its
    // reserved identity is available to the configured source. Re-enabling the
    // fallback later makes any transition of that source collide with it.
    manager.setDeepSkyCatalogPresetIndex(1);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.instanceId = QStringLiteral("bundled-deep-sky");
    accepted.title = QStringLiteral("Composition Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Composition Accepted Star")));

    manager.setDeepSkyCatalogPresetIndex(0);

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Composition Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The catalog parsed but the runtime refused the collection transition:
    // the operation reports it and the accepted snapshot stays active.
    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("rejected")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Composition Accepted Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Composition Parsed Star")));

    // Persisting later serializes the accepted facts, not the parsed attempt.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.moveSource(instanceId, 0);
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Composition Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(record->payload, acceptedPayload);
}

QTEST_GUILESS_MAIN(SkyCatalogManagerImportTests)

#include "SkyCatalogManagerImportTests.moc"
