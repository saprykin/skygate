#include "AsyncTestSupport.hpp"
#include "CatalogCoordinator.hpp"
#include "CatalogTestPayloads.hpp"
#include "catalog/CatalogDownloadService.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "LogCapture.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QNetworkReply>

#include <algorithm>
#include <memory>
#include <utility>

using namespace skygate::ui::tests;

namespace {

FakeNetworkReply* onlyIssuedReply(FakeNetworkAccessManager& networkAccessManager)
{
    const QList<FakeNetworkReply*> replies = networkAccessManager.issuedReplies();
    if (replies.size() != 1) {
        QTest::qFail(
            qPrintable(QString("Expected 1 issued reply, got %1").arg(replies.size())),
            __FILE__,
            __LINE__
        );
        return nullptr;
    }
    return replies.front();
}

void waitForFakeReplyFinished(FakeNetworkReply* reply)
{
    QVERIFY(reply != nullptr);
    if (!reply->isFinished()) {
        QSignalSpy finishedSpy(reply, &QNetworkReply::finished);
        if (!reply->isFinished()) {
            QVERIFY(finishedSpy.wait(1000));
        }
    }
    QVERIFY(reply->isFinished());
}

}  // namespace

class CatalogDownloadWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidUrlCompletesWithoutNetworkRequest();
    void downloadServiceLogsStartAndSuccessAtInfoLevel();
    void downloadServiceRetriesUntilFirstSuccessfulPayload();
    void downloadServiceRejectsEmptyAndOversizedPayloads();
    void downloadServiceDoesNotCallDestroyedCallbackContext();
    void downloadServiceStopsRetryAfterDestroyedCallbackContext();
    void downloadServiceRetriesAfterAbortedReply();
    void downloadServiceCompletesConcurrentRequestsIndependently();
    void downloadServiceConcurrentFallbackChainsRemainIsolated();
    void coordinatorParsesSuccessfulDownloadedCatalog();
    void coordinatorReportsParseFailureWithSourceUrl();
    void importWorkflowParsesConstellationPayloads();
    void importWorkflowDoesNotCompleteConstellationAfterContextDestroyed();
    void importWorkflowFallsBackForMalformedConstellationPayload();
    void importWorkflowRejectsDeepSkyCatalogWithoutDsos();
    void importWorkflowReportsDeepSkyObjectCount();
};

void CatalogDownloadWorkflowTests::invalidUrlCompletesWithoutNetworkRequest()
{
    FakeNetworkAccessManager networkAccessManager;
    CatalogDownloadService service(&networkAccessManager);

    bool completed = false;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog download aborted: no valid source URLs"
    );
    service.downloadFirstSuccessfulFromUrls(
        {"  "},
        this,
        {},
        [&completed](CatalogDownloadService::DownloadResult result) {
            completed = true;
            QVERIFY(result.payload.isEmpty());
            QCOMPARE(result.errorText, QString("Catalog: Invalid URL"));
        }
    );

    QVERIFY(completed);
    QVERIFY(networkAccessManager.requestedUrls().isEmpty());
}

void CatalogDownloadWorkflowTests::downloadServiceLogsStartAndSuccessAtInfoLevel()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/catalog.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    LogCapture capture(QtInfoMsg);
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            {"https://example.test/catalog.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(!finalResult.payload.isEmpty());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("skygate.catalog.download")));
    QVERIFY(messages.contains(QStringLiteral("Catalog download started: sources 1")));
    QVERIFY(messages.contains(QStringLiteral("Catalog download succeeded: https://example.test/catalog.csv")));
    QVERIFY(messages.contains(QStringLiteral("bytes")));
}

void CatalogDownloadWorkflowTests::downloadServiceRetriesUntilFirstSuccessfulPayload()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/fail.csv",
        {.error = QNetworkReply::ContentNotFoundError, .errorText = "not found", .httpStatusCode = 404}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/ok.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source failed https://example.test/fail.csv Unknown error HTTP 404"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            {"https://example.test/fail.csv", "https://example.test/ok.csv"},
            this,
            [&statuses](const QString& status) {
                statuses.push_back(status);
            },
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QCOMPARE(networkAccessManager.requestedUrls().size(), 2);
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/ok.csv"));
    QVERIFY(finalResult.errorText.isEmpty());
    QVERIFY(finalResult.payload.startsWith("id,hip"));
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("failed", Qt::CaseInsensitive)
            || status.contains("HTTP 404", Qt::CaseInsensitive);
    }));
}

void CatalogDownloadWorkflowTests::downloadServiceRejectsEmptyAndOversizedPayloads()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/empty.csv", {});
    networkAccessManager.enqueueResponse(
        "https://example.test/too-large.csv",
        {.payload = QByteArray(static_cast<int>((128U << 20U) + 1U), 'x')}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source returned empty data https://example.test/empty.csv"
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source exceeded size limit https://example.test/too-large.csv 134217729"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            {"https://example.test/empty.csv", "https://example.test/too-large.csv"},
            this,
            [&statuses](const QString& status) {
                statuses.push_back(status);
            },
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.payload.isEmpty());
    QVERIFY(finalResult.errorText.contains("file too large"));
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("returned empty data");
    }));
}

void CatalogDownloadWorkflowTests::downloadServiceDoesNotCallDestroyedCallbackContext()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/slow.csv",
        {
            .payload = sampleHygCsvPayload({1, 1, "Slow", "1.0", "2.0", "3.0"}),
            .delayMs = 20
        }
    );
    CatalogDownloadService service(&networkAccessManager);

    bool statusCalled = false;
    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/slow.csv"},
        callbackContext.get(),
        [&statusCalled](const QString&) {
            statusCalled = true;
        },
        [&completionCalled](CatalogDownloadService::DownloadResult) {
            completionCalled = true;
        }
    );

    QVERIFY(statusCalled);
    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    callbackContext.reset();
    waitForFakeReplyFinished(reply);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
    QCOMPARE(networkAccessManager.finishedUrls(), QStringList({"https://example.test/slow.csv"}));
    QVERIFY(networkAccessManager.abortedUrls().isEmpty());
}

void CatalogDownloadWorkflowTests::downloadServiceStopsRetryAfterDestroyedCallbackContext()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/slow-fail.csv",
        {
            .error = QNetworkReply::ContentNotFoundError,
            .errorText = "not found",
            .httpStatusCode = 404,
            .delayMs = 20
        }
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/should-not-start.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Unexpected", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/slow-fail.csv", "https://example.test/should-not-start.csv"},
        callbackContext.get(),
        {},
        [&completionCalled](CatalogDownloadService::DownloadResult) {
            completionCalled = true;
        }
    );

    QCOMPARE(networkAccessManager.requestedUrls().size(), 1);
    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    callbackContext.reset();
    waitForFakeReplyFinished(reply);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
    QCOMPARE(networkAccessManager.requestedUrls().size(), 1);
    QCOMPARE(networkAccessManager.finishedUrls(), QStringList({"https://example.test/slow-fail.csv"}));
    QVERIFY(networkAccessManager.abortedUrls().isEmpty());
}

void CatalogDownloadWorkflowTests::downloadServiceRetriesAfterAbortedReply()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/cancelled.csv",
        {
            .payload = sampleHygCsvPayload({1, 1, "Cancelled", "1.0", "2.0", "3.0"}),
            .delayMs = 500
        }
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/ok-after-cancel.csv",
        {.payload = sampleHygCsvPayload({2, 2, "Recovered", "4.0", "5.0", "6.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    QStringList statuses;
    bool completed = false;
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/cancelled.csv", "https://example.test/ok-after-cancel.csv"},
        this,
        [&statuses](const QString& status) {
            statuses.push_back(status);
        },
        [&finalResult, &completed](CatalogDownloadService::DownloadResult result) {
            finalResult = std::move(result);
            completed = true;
        }
    );

    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/cancelled.csv"}));
    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source failed https://example.test/cancelled.csv Operation canceled HTTP 0"
    );
    reply->abort();
    QVERIFY(reply->isAborted());

    QTRY_VERIFY(completed);
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/ok-after-cancel.csv"));
    QVERIFY(finalResult.payload.contains("Recovered"));
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({
        "https://example.test/cancelled.csv",
        "https://example.test/ok-after-cancel.csv"
    }));
    QCOMPARE(networkAccessManager.finishedUrls().front(), QString("https://example.test/cancelled.csv"));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({"https://example.test/cancelled.csv"}));
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("Operation canceled");
    }));
}

void CatalogDownloadWorkflowTests::downloadServiceCompletesConcurrentRequestsIndependently()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/first.csv",
        {
            .payload = sampleHygCsvPayload({1, 11, "First", "1.0", "2.0", "3.0"}),
            .delayMs = 25
        }
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/second.csv",
        {.payload = sampleHygCsvPayload({2, 22, "Second", "4.0", "5.0", "6.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult firstResult;
    CatalogDownloadService::DownloadResult secondResult;
    bool firstCompleted = false;
    bool secondCompleted = false;
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/first.csv"},
        this,
        {},
        [&firstResult, &firstCompleted](CatalogDownloadService::DownloadResult result) {
            firstResult = std::move(result);
            firstCompleted = true;
        }
    );
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/second.csv"},
        this,
        {},
        [&secondResult, &secondCompleted](CatalogDownloadService::DownloadResult result) {
            secondResult = std::move(result);
            secondCompleted = true;
        }
    );

    QTRY_VERIFY(firstCompleted && secondCompleted);
    QCOMPARE(firstResult.sourceUrl, QString("https://example.test/first.csv"));
    QCOMPARE(secondResult.sourceUrl, QString("https://example.test/second.csv"));
    QVERIFY(firstResult.payload.contains("First"));
    QVERIFY(secondResult.payload.contains("Second"));
    QCOMPARE(networkAccessManager.requestedUrls().size(), 2);
}

void CatalogDownloadWorkflowTests::downloadServiceConcurrentFallbackChainsRemainIsolated()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/a-fail.csv",
        {
            .error = QNetworkReply::ContentNotFoundError,
            .errorText = "not found",
            .httpStatusCode = 404,
            .delayMs = 20
        }
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/a-ok.csv",
        {.payload = sampleHygCsvPayload({1, 101, "Alpha", "1.0", "2.0", "3.0"})}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/b-fail.csv",
        {.payload = QByteArray()}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/b-ok.csv",
        {
            .payload = sampleHygCsvPayload({2, 202, "Beta", "4.0", "5.0", "6.0"}),
            .delayMs = 10
        }
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult firstResult;
    CatalogDownloadService::DownloadResult secondResult;
    bool firstCompleted = false;
    bool secondCompleted = false;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source returned empty data https://example.test/b-fail.csv"
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog source failed https://example.test/a-fail.csv Unknown error HTTP 404"
    );
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/a-fail.csv", "https://example.test/a-ok.csv"},
        this,
        {},
        [&firstResult, &firstCompleted](CatalogDownloadService::DownloadResult result) {
            firstResult = std::move(result);
            firstCompleted = true;
        }
    );
    service.downloadFirstSuccessfulFromUrls(
        {"https://example.test/b-fail.csv", "https://example.test/b-ok.csv"},
        this,
        {},
        [&secondResult, &secondCompleted](CatalogDownloadService::DownloadResult result) {
            secondResult = std::move(result);
            secondCompleted = true;
        }
    );

    QTRY_VERIFY(firstCompleted && secondCompleted);
    QCOMPARE(firstResult.sourceUrl, QString("https://example.test/a-ok.csv"));
    QCOMPARE(secondResult.sourceUrl, QString("https://example.test/b-ok.csv"));
    QVERIFY(firstResult.payload.contains("Alpha"));
    QVERIFY(secondResult.payload.contains("Beta"));
    QCOMPARE(networkAccessManager.requestedUrls().size(), 4);
}

void CatalogDownloadWorkflowTests::coordinatorParsesSuccessfulDownloadedCatalog()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/catalog.csv",
        {.payload = sampleHygCsvPayload()}
    );
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QStringList statuses;
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/catalog.csv"},
            this,
            [&statuses](const QString& status) {
                statuses.push_back(status);
            },
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    QCOMPARE(finalResult.diagnostics.parsedBodyCount, 1U);
    QVERIFY(finalResult.errorText.isEmpty());
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.startsWith("Catalog: Processing");
    }));
}

void CatalogDownloadWorkflowTests::coordinatorReportsParseFailureWithSourceUrl()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/bad.csv",
        {.payload = sampleHygCsvPayload({1, 42, "Bad", "not-ra", "2.0", "3.0"})}
    );
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(
        QtWarningMsg,
        "HYG CSV skipped 1 rows with invalid numeric values; samples: row 2 ra='not-ra' dec='2.0' mag='3.0'"
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "HYG CSV parse failed: HYG CSV payload does not contain any valid star rows."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/bad.csv parse failed: invalid HYG CSV payload (HYG CSV payload does not contain any valid star rows.)"
    );
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/bad.csv"},
            this,
            [&statuses](const QString& status) {
                statuses.push_back(status);
            },
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("https://example.test/bad.csv"));
    QVERIFY(finalResult.errorText.contains("parse failed"));
    QCOMPARE(statuses.back(), finalResult.errorText);
}

void CatalogDownloadWorkflowTests::importWorkflowParsesConstellationPayloads()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/index.json",
        {.payload = sampleConstellationIndexJsonPayload()}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyConstellationLineImportResult finalResult;
    QStringList statuses;
    LogCapture capture(QtInfoMsg);
    runAsync([&](QEventLoop& loop) {
        workflow.downloadConstellationLines(
            {"https://example.test/index.json"},
            this,
            [&statuses](const QString& status) {
                statuses.push_back(status);
            },
            [&finalResult, &loop](
                skygate::ui::internal::SkyConstellationLineImportResult result
            ) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QCOMPARE(finalResult.constellationCount, 1U);
    QCOMPARE(finalResult.lineRefs.size(), 2U);
    QCOMPARE(finalResult.labelRefs.size(), 1U);
    QVERIFY(finalResult.hasCustomLines());
    QCOMPARE(finalResult.statusSuffix, QString("2 segments"));
    QVERIFY(std::none_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("failed", Qt::CaseInsensitive);
    }));
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Constellation lines parsed: 2 segments 1 labels 1 constellations")));
}

void CatalogDownloadWorkflowTests::importWorkflowDoesNotCompleteConstellationAfterContextDestroyed()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/slow-index.json",
        {
            .payload = sampleConstellationIndexJsonPayload(),
            .delayMs = 20
        }
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    workflow.downloadConstellationLines(
        {"https://example.test/slow-index.json"},
        callbackContext.get(),
        {},
        [&completionCalled](skygate::ui::internal::SkyConstellationLineImportResult) {
            completionCalled = true;
        }
    );

    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    callbackContext.reset();
    waitForFakeReplyFinished(reply);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
    QCOMPARE(networkAccessManager.finishedUrls(), QStringList({"https://example.test/slow-index.json"}));
    QVERIFY(networkAccessManager.abortedUrls().isEmpty());
}

void CatalogDownloadWorkflowTests::importWorkflowFallsBackForMalformedConstellationPayload()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/bad-index.json",
        {.payload = "not-json"}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyConstellationLineImportResult finalResult;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Constellation line parse failed; using bundled fallback. Payload preview: not-json"
    );
    runAsync([&](QEventLoop& loop) {
        workflow.downloadConstellationLines(
            {"https://example.test/bad-index.json"},
            this,
            {},
            [&finalResult, &loop](
                skygate::ui::internal::SkyConstellationLineImportResult result
            ) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(!finalResult.hasCustomLines());
    QVERIFY(finalResult.labelRefs.empty());
    QVERIFY(finalResult.statusSuffix.contains("fallback default"));
    QVERIFY(finalResult.statusSuffix.contains("parse failed"));
    QVERIFY(finalResult.statusSuffix.contains("not-json"));
}

void CatalogDownloadWorkflowTests::importWorkflowRejectsDeepSkyCatalogWithoutDsos()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/stars.csv",
        {.payload = sampleHygCsvPayload()}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyDeepSkyCatalogImportResult finalResult;
    runAsync([&](QEventLoop& loop) {
        workflow.downloadDeepSkyCatalog(
            {"https://example.test/stars.csv"},
            "HYG",
            this,
            {},
            [&finalResult, &loop](
                skygate::ui::internal::SkyDeepSkyCatalogImportResult result
            ) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QCOMPARE(finalResult.sourceLabel, QString("HYG"));
    QCOMPARE(finalResult.foundObjectCount, 1U);
    QCOMPARE(finalResult.errorText, QString("Catalog: Downloaded deep-sky catalog contains no DSOs"));
}

void CatalogDownloadWorkflowTests::importWorkflowReportsDeepSkyObjectCount()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/open-ngc.csv",
        {.payload = sampleOpenNgcCsvPayload()}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyDeepSkyCatalogImportResult finalResult;
    runAsync([&](QEventLoop& loop) {
        workflow.downloadDeepSkyCatalog(
            {"https://example.test/open-ngc.csv"},
            "OpenNGC",
            this,
            {},
            [&finalResult, &loop](
                skygate::ui::internal::SkyDeepSkyCatalogImportResult result
            ) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    QCOMPARE(finalResult.sourceLabel, QString("OpenNGC"));
    QCOMPARE(finalResult.foundObjectCount, 1U);
    QVERIFY(finalResult.errorText.isEmpty());
}

QTEST_GUILESS_MAIN(CatalogDownloadWorkflowTests)

#include "CatalogDownloadWorkflowTests.moc"
