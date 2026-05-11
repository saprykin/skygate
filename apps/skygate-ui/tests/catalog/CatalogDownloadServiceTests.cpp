#include "AsyncTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "catalog/CatalogDownloadService.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "LogCapture.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QNetworkReply>

#include <algorithm>
#include <memory>
#include <utility>

using namespace skygate::ui::tests;

class CatalogDownloadServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidUrlCompletesWithoutNetworkRequest();
    void logsStartAndSuccessAtInfoLevel();
    void retriesUntilFirstSuccessfulPayload();
    void rejectsEmptyAndOversizedPayloads();
    void doesNotCallDestroyedCallbackContext();
    void stopsRetryAfterDestroyedCallbackContext();
    void retriesAfterAbortedReply();
    void completesConcurrentRequestsIndependently();
    void concurrentFallbackChainsRemainIsolated();
};

void CatalogDownloadServiceTests::invalidUrlCompletesWithoutNetworkRequest()
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

void CatalogDownloadServiceTests::logsStartAndSuccessAtInfoLevel()
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

void CatalogDownloadServiceTests::retriesUntilFirstSuccessfulPayload()
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

void CatalogDownloadServiceTests::rejectsEmptyAndOversizedPayloads()
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

void CatalogDownloadServiceTests::doesNotCallDestroyedCallbackContext()
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

void CatalogDownloadServiceTests::stopsRetryAfterDestroyedCallbackContext()
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

void CatalogDownloadServiceTests::retriesAfterAbortedReply()
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

void CatalogDownloadServiceTests::completesConcurrentRequestsIndependently()
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

void CatalogDownloadServiceTests::concurrentFallbackChainsRemainIsolated()
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

QTEST_GUILESS_MAIN(CatalogDownloadServiceTests)

#include "CatalogDownloadServiceTests.moc"
