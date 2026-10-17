#include "catalog/CatalogDownloadCancellation.hpp"
#include "catalog/CatalogDownloadService.hpp"
#include "support/AsyncTestSupport.hpp"
#include "support/CatalogDownloadWorkflowTestSupport.hpp"
#include "support/CatalogTestPayloads.hpp"
#include "support/FakeNetworkAccessManager.hpp"
#include "support/LogCapture.hpp"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <utility>

using namespace skygate::ui::tests;

namespace {

constexpr qsizetype kCatalogByteLimit = 128U << 20U;
constexpr qsizetype kOverLimitPayloadBytes = kCatalogByteLimit + 4 * 1024 * 1024;

std::shared_ptr<CatalogDownloadCancellation> newCancellation()
{
    return std::make_shared<CatalogDownloadCancellation>();
}

// Captures fixture state while the reply still exists: the service schedules
// reply deletion after completion, so the values must be read from a finished
// handler.
struct ReplyAbortState final {
    bool aborted = false;
    qsizetype producedBytes = 0;
};

std::shared_ptr<ReplyAbortState> watchAbortState(FakeNetworkReply* reply)
{
    auto state = std::make_shared<ReplyAbortState>();
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, state]() {
        state->aborted = reply->isAborted();
        state->producedBytes = reply->producedBytesWhenAborted();
    });
    return state;
}

}  // namespace

class CatalogDownloadServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void invalidUrlCompletesWithoutNetworkRequest();
    void logsStartAndSuccessAtInfoLevel();
    void retriesUntilFirstSuccessfulPayload();
    void rejectsEmptyAndOversizedPayloads();
    void acceptsPayloadAtExactLimit();
    void rejectsPayloadOneByteOverLimitWithoutLength();
    void rejectsPayloadWithUnderstatedLength();
    void rejectsExcessiveDeclaredLength();
    void fallsBackAfterOversizedCandidate();
    void doesNotCallDestroyedCallbackContext();
    void stopsRetryAfterDestroyedCallbackContext();
    void fallsBackWhenReplyAbortsWithoutCancellation();
    void explicitCancellationStopsFallbackChain();
    void statusCancellationStopsFallbackCandidate();
    void completesConcurrentRequestsIndependently();
    void concurrentFallbackChainsRemainIsolated();
    void successReleasesCompletionCapture();
    void exhaustedRetriesReleaseCompletionCapture();
    void invalidCandidatesReleaseCompletionCapture();
    void callbackContextDestructionReleasesCapture();
};

void CatalogDownloadServiceTests::invalidUrlCompletesWithoutNetworkRequest()
{
    FakeNetworkAccessManager networkAccessManager;
    CatalogDownloadService service(&networkAccessManager);

    bool completed = false;
    QTest::ignoreMessage(QtWarningMsg, "Catalog download aborted: no valid source URLs");
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(), {"  "}, this, {}, [&completed](CatalogDownloadService::DownloadResult result) {
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
        "https://example.test/catalog.csv", {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    LogCapture capture(QtInfoMsg);
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
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
        "https://example.test/ok.csv", {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(QtWarningMsg, "Catalog source failed https://example.test/fail.csv Unknown error HTTP 404");
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/fail.csv", "https://example.test/ok.csv"},
            this,
            [&statuses](const QString& status) { statuses.push_back(status); },
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
        return status.contains("failed", Qt::CaseInsensitive) || status.contains("HTTP 404", Qt::CaseInsensitive);
    }));
}

void CatalogDownloadServiceTests::rejectsEmptyAndOversizedPayloads()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/empty.csv", {});
    networkAccessManager.enqueueResponse(
        "https://example.test/too-large.csv", {.payload = QByteArray(static_cast<int>((128U << 20U) + 1U), 'x')}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(QtWarningMsg, "Catalog source returned empty data https://example.test/empty.csv");
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source declared size over limit https://example.test/too-large.csv 134217729"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/empty.csv", "https://example.test/too-large.csv"},
            this,
            [&statuses](const QString& status) { statuses.push_back(status); },
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

void CatalogDownloadServiceTests::acceptsPayloadAtExactLimit()
{
    constexpr qsizetype kChunkBytes = 4 * 1024 * 1024;

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/at-limit.csv", {.payload = QByteArray(kCatalogByteLimit, 'x'), .chunkSize = kChunkBytes}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/at-limit.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QCOMPARE(finalResult.payload.size(), kCatalogByteLimit);
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/at-limit.csv"));
    QVERIFY(finalResult.errorText.isEmpty());
    QVERIFY(networkAccessManager.abortedUrls().isEmpty());
}

void CatalogDownloadServiceTests::rejectsPayloadOneByteOverLimitWithoutLength()
{
    constexpr qsizetype kChunkBytes = 8 * 1024 * 1024;
    constexpr qsizetype kPayloadBytes = kCatalogByteLimit + 1;

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/over-limit.csv",
        {.payload = QByteArray(kPayloadBytes, 'x'), .chunkSize = kChunkBytes, .includeContentLength = false}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    std::shared_ptr<ReplyAbortState> abortState;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source exceeded size limit https://example.test/over-limit.csv 134217729"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/over-limit.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
        abortState = watchAbortState(onlyIssuedReply(networkAccessManager));
    });

    QVERIFY(finalResult.payload.isEmpty());
    QVERIFY(finalResult.errorText.contains("file too large"));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({"https://example.test/over-limit.csv"}));
    QVERIFY(abortState->aborted);
    QCOMPARE(abortState->producedBytes, kPayloadBytes);
}

void CatalogDownloadServiceTests::rejectsPayloadWithUnderstatedLength()
{
    constexpr qsizetype kChunkBytes = 1024 * 1024;
    constexpr qint64 kDeclaredBytes = 4096;

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/understated.csv",
        {.payload = QByteArray(kOverLimitPayloadBytes, 'x'),
         .chunkSize = kChunkBytes,
         .declaredContentLength = kDeclaredBytes}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    std::shared_ptr<ReplyAbortState> abortState;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source exceeded size limit https://example.test/understated.csv 135266304"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/understated.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
        abortState = watchAbortState(onlyIssuedReply(networkAccessManager));
    });

    QVERIFY(finalResult.payload.isEmpty());
    QVERIFY(finalResult.errorText.contains("file too large"));
    QVERIFY(abortState->aborted);
    QVERIFY(abortState->producedBytes > kCatalogByteLimit);
    QVERIFY(abortState->producedBytes < kOverLimitPayloadBytes);
}

void CatalogDownloadServiceTests::rejectsExcessiveDeclaredLength()
{
    constexpr qsizetype kChunkBytes = 512;
    constexpr qsizetype kPayloadBytes = 4096;
    constexpr qint64 kDeclaredBytes = static_cast<qint64>(kCatalogByteLimit) + 1;

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/declared-too-large.csv",
        {.payload = QByteArray(kPayloadBytes, 'x'), .chunkSize = kChunkBytes, .declaredContentLength = kDeclaredBytes}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    std::shared_ptr<ReplyAbortState> abortState;
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source declared size over limit https://example.test/declared-too-large.csv 134217729"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            newCancellation(),
            {"https://example.test/declared-too-large.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
        abortState = watchAbortState(onlyIssuedReply(networkAccessManager));
    });

    QVERIFY(finalResult.payload.isEmpty());
    QVERIFY(finalResult.errorText.contains("file too large"));
    QVERIFY(abortState->aborted);
    QCOMPARE(abortState->producedBytes, kChunkBytes);
    QVERIFY(abortState->producedBytes < kPayloadBytes);
}

void CatalogDownloadServiceTests::fallsBackAfterOversizedCandidate()
{
    constexpr qsizetype kChunkBytes = 1024 * 1024;

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/oversized.csv",
        {.payload = QByteArray(kOverLimitPayloadBytes, 'x'), .chunkSize = kChunkBytes}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/ok.csv", {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult finalResult;
    std::shared_ptr<ReplyAbortState> abortState;
    const auto cancellation = std::make_shared<CatalogDownloadCancellation>();
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source declared size over limit https://example.test/oversized.csv 138412032"
    );
    runAsync([&](QEventLoop& loop) {
        service.downloadFirstSuccessfulFromUrls(
            cancellation,
            {"https://example.test/oversized.csv", "https://example.test/ok.csv"},
            this,
            {},
            [&finalResult, &loop](CatalogDownloadService::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
        abortState = watchAbortState(onlyIssuedReply(networkAccessManager));
    });

    QCOMPARE(
        networkAccessManager.requestedUrls(),
        QStringList({"https://example.test/oversized.csv", "https://example.test/ok.csv"})
    );
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({"https://example.test/oversized.csv"}));
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/ok.csv"));
    QVERIFY(finalResult.errorText.isEmpty());
    QVERIFY(finalResult.payload.contains("Alpha"));
    QVERIFY(abortState->aborted);
    QVERIFY(abortState->producedBytes < kOverLimitPayloadBytes);
    QVERIFY(cancellation != nullptr);
    QVERIFY(!cancellation->isCanceled());
}

void CatalogDownloadServiceTests::doesNotCallDestroyedCallbackContext()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/slow.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Slow", "1.0", "2.0", "3.0"}), .delayMs = 20}
    );
    CatalogDownloadService service(&networkAccessManager);

    bool statusCalled = false;
    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
        {"https://example.test/slow.csv"},
        callbackContext.get(),
        [&statusCalled](const QString&) { statusCalled = true; },
        [&completionCalled](CatalogDownloadService::DownloadResult) { completionCalled = true; }
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
        {.error = QNetworkReply::ContentNotFoundError, .errorText = "not found", .httpStatusCode = 404, .delayMs = 20}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/should-not-start.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Unexpected", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
        {"https://example.test/slow-fail.csv", "https://example.test/should-not-start.csv"},
        callbackContext.get(),
        {},
        [&completionCalled](CatalogDownloadService::DownloadResult) { completionCalled = true; }
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

void CatalogDownloadServiceTests::fallsBackWhenReplyAbortsWithoutCancellation()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/cancelled.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Cancelled", "1.0", "2.0", "3.0"}), .delayMs = 500}
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
        newCancellation(),
        {"https://example.test/cancelled.csv", "https://example.test/ok-after-cancel.csv"},
        this,
        [&statuses](const QString& status) { statuses.push_back(status); },
        [&finalResult, &completed](CatalogDownloadService::DownloadResult result) {
            finalResult = std::move(result);
            completed = true;
        }
    );

    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/cancelled.csv"}));
    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source failed https://example.test/cancelled.csv Operation canceled HTTP 0"
    );
    reply->abort();
    QVERIFY(reply->isAborted());

    QTRY_VERIFY(completed);
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/ok-after-cancel.csv"));
    QVERIFY(finalResult.payload.contains("Recovered"));
    QCOMPARE(
        networkAccessManager.requestedUrls(),
        QStringList({"https://example.test/cancelled.csv", "https://example.test/ok-after-cancel.csv"})
    );
    QCOMPARE(networkAccessManager.finishedUrls().front(), QString("https://example.test/cancelled.csv"));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({"https://example.test/cancelled.csv"}));
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("Operation canceled");
    }));
}

void CatalogDownloadServiceTests::explicitCancellationStopsFallbackChain()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/pending.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Pending", "1.0", "2.0", "3.0"}), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/after-cancel.csv",
        {.payload = sampleHygCsvPayload({2, 2, "After Cancel", "4.0", "5.0", "6.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    int completionCount = 0;
    QStringList statuses;
    const auto cancellation = std::make_shared<CatalogDownloadCancellation>();
    service.downloadFirstSuccessfulFromUrls(
        cancellation,
        {"https://example.test/pending.csv", "https://example.test/after-cancel.csv"},
        this,
        [&statuses](const QString& status) { statuses.push_back(status); },
        [&completionCount](CatalogDownloadService::DownloadResult) { ++completionCount; }
    );
    QVERIFY(cancellation != nullptr);
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/pending.csv"}));

    cancellation->cancel();

    QVERIFY(cancellation->isCanceled());
    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    QVERIFY(reply->isAborted());
    waitForFakeReplyFinished(reply);
    QCoreApplication::processEvents();

    QCOMPARE(completionCount, 0);
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/pending.csv"}));
    QCOMPARE(networkAccessManager.abortedUrls(), QStringList({"https://example.test/pending.csv"}));
    QVERIFY(std::none_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.contains("failed", Qt::CaseInsensitive);
    }));
}

void CatalogDownloadServiceTests::statusCancellationStopsFallbackCandidate()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/first-fail.csv",
        {.error = QNetworkReply::ContentNotFoundError,
         .errorText = "not found",
         .httpStatusCode = 404,
         .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/fallback.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Fallback", "1.0", "2.0", "3.0"}), .manualFinish = true}
    );
    CatalogDownloadService service(&networkAccessManager);

    int completionCount = 0;
    const auto cancellation = std::make_shared<CatalogDownloadCancellation>();
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog source failed https://example.test/first-fail.csv Unknown error HTTP 404"
    );
    service.downloadFirstSuccessfulFromUrls(
        cancellation,
        {"https://example.test/first-fail.csv", "https://example.test/fallback.csv"},
        this,
        [&cancellation](const QString& status) {
            if (status.contains("fallback.csv")) {
                cancellation->cancel();
            }
        },
        [&completionCount](CatalogDownloadService::DownloadResult) { ++completionCount; }
    );
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/first-fail.csv"}));

    FakeNetworkReply* firstReply = onlyIssuedReply(networkAccessManager);
    firstReply->finishNow();
    QCoreApplication::processEvents();

    QVERIFY(cancellation->isCanceled());
    QCOMPARE(completionCount, 0);
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/first-fail.csv"}));
    QCOMPARE(networkAccessManager.issuedReplies().size(), 1);
    QVERIFY(networkAccessManager.abortedUrls().isEmpty());
}

void CatalogDownloadServiceTests::completesConcurrentRequestsIndependently()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/first.csv",
        {.payload = sampleHygCsvPayload({1, 11, "First", "1.0", "2.0", "3.0"}), .delayMs = 25}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/second.csv", {.payload = sampleHygCsvPayload({2, 22, "Second", "4.0", "5.0", "6.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult firstResult;
    CatalogDownloadService::DownloadResult secondResult;
    bool firstCompleted = false;
    bool secondCompleted = false;
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
        {"https://example.test/first.csv"},
        this,
        {},
        [&firstResult, &firstCompleted](CatalogDownloadService::DownloadResult result) {
            firstResult = std::move(result);
            firstCompleted = true;
        }
    );
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
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
        {.error = QNetworkReply::ContentNotFoundError, .errorText = "not found", .httpStatusCode = 404, .delayMs = 20}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/a-ok.csv", {.payload = sampleHygCsvPayload({1, 101, "Alpha", "1.0", "2.0", "3.0"})}
    );
    networkAccessManager.enqueueResponse("https://example.test/b-fail.csv", {.payload = QByteArray()});
    networkAccessManager.enqueueResponse(
        "https://example.test/b-ok.csv",
        {.payload = sampleHygCsvPayload({2, 202, "Beta", "4.0", "5.0", "6.0"}), .delayMs = 10}
    );
    CatalogDownloadService service(&networkAccessManager);

    CatalogDownloadService::DownloadResult firstResult;
    CatalogDownloadService::DownloadResult secondResult;
    bool firstCompleted = false;
    bool secondCompleted = false;
    QTest::ignoreMessage(QtWarningMsg, "Catalog source returned empty data https://example.test/b-fail.csv");
    QTest::ignoreMessage(QtWarningMsg, "Catalog source failed https://example.test/a-fail.csv Unknown error HTTP 404");
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
        {"https://example.test/a-fail.csv", "https://example.test/a-ok.csv"},
        this,
        {},
        [&firstResult, &firstCompleted](CatalogDownloadService::DownloadResult result) {
            firstResult = std::move(result);
            firstCompleted = true;
        }
    );
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
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

void CatalogDownloadServiceTests::successReleasesCompletionCapture()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/catalog.csv", {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    const auto capture = std::make_shared<int>(0);
    int completionCount = 0;
    CatalogDownloadService::CompletionHandler completionHandler =
        [capture, &completionCount](CatalogDownloadService::DownloadResult) { ++completionCount; };
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(), {"https://example.test/catalog.csv"}, this, {}, completionHandler
    );
    completionHandler = {};

    QTRY_COMPARE(completionCount, 1);
    QTRY_COMPARE(capture.use_count(), 1L);
    QCoreApplication::processEvents();
    QCOMPARE(completionCount, 1);
}

void CatalogDownloadServiceTests::exhaustedRetriesReleaseCompletionCapture()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/a-fail.csv",
        {.error = QNetworkReply::ContentNotFoundError, .errorText = "not found", .httpStatusCode = 404}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/b-fail.csv",
        {.error = QNetworkReply::ContentNotFoundError, .errorText = "not found", .httpStatusCode = 404}
    );
    CatalogDownloadService service(&networkAccessManager);

    QTest::ignoreMessage(QtWarningMsg, "Catalog source failed https://example.test/a-fail.csv Unknown error HTTP 404");
    QTest::ignoreMessage(QtWarningMsg, "Catalog source failed https://example.test/b-fail.csv Unknown error HTTP 404");

    const auto capture = std::make_shared<int>(0);
    int completionCount = 0;
    QString errorText;
    CatalogDownloadService::CompletionHandler completionHandler =
        [capture, &completionCount, &errorText](CatalogDownloadService::DownloadResult result) {
            ++completionCount;
            errorText = result.errorText;
        };
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(),
        {"https://example.test/a-fail.csv", "https://example.test/b-fail.csv"},
        this,
        {},
        completionHandler
    );
    completionHandler = {};

    QTRY_COMPARE(completionCount, 1);
    QTRY_COMPARE(capture.use_count(), 1L);
    QCoreApplication::processEvents();
    QCOMPARE(completionCount, 1);
    QCOMPARE(errorText, QString("Catalog: Source https://example.test/b-fail.csv failed (Unknown error, HTTP 404)"));
    QCOMPARE(networkAccessManager.requestedUrls().size(), 2);
}

void CatalogDownloadServiceTests::invalidCandidatesReleaseCompletionCapture()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/ok.csv", {.payload = sampleHygCsvPayload({1, 1, "Alpha", "1.0", "2.0", "3.0"})}
    );
    CatalogDownloadService service(&networkAccessManager);

    QTest::ignoreMessage(QtWarningMsg, "Skipping invalid catalog source URL invalid url");

    const auto capture = std::make_shared<int>(0);
    int completionCount = 0;
    QString sourceUrl;
    CatalogDownloadService::CompletionHandler completionHandler =
        [capture, &completionCount, &sourceUrl](CatalogDownloadService::DownloadResult result) {
            ++completionCount;
            sourceUrl = result.sourceUrl;
        };
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(), {"invalid url", "  ", "https://example.test/ok.csv"}, this, {}, completionHandler
    );
    completionHandler = {};

    QTRY_COMPARE(completionCount, 1);
    QTRY_COMPARE(capture.use_count(), 1L);
    QCoreApplication::processEvents();
    QCOMPARE(completionCount, 1);
    QCOMPARE(sourceUrl, QString("https://example.test/ok.csv"));
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList({"https://example.test/ok.csv"}));
}

void CatalogDownloadServiceTests::callbackContextDestructionReleasesCapture()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/slow.csv",
        {.payload = sampleHygCsvPayload({1, 1, "Slow", "1.0", "2.0", "3.0"}), .delayMs = 20}
    );
    CatalogDownloadService service(&networkAccessManager);

    const auto capture = std::make_shared<int>(0);
    int completionCount = 0;
    CatalogDownloadService::CompletionHandler completionHandler =
        [capture, &completionCount](CatalogDownloadService::DownloadResult) { ++completionCount; };
    auto callbackContext = std::make_unique<QObject>();
    service.downloadFirstSuccessfulFromUrls(
        newCancellation(), {"https://example.test/slow.csv"}, callbackContext.get(), {}, completionHandler
    );
    completionHandler = {};

    FakeNetworkReply* reply = onlyIssuedReply(networkAccessManager);
    callbackContext.reset();
    waitForFakeReplyFinished(reply);
    QCoreApplication::processEvents();

    QCOMPARE(completionCount, 0);
    QTRY_COMPARE(capture.use_count(), 1L);
    QTRY_VERIFY(networkAccessManager.issuedReplies().isEmpty());
    QCOMPARE(networkAccessManager.finishedUrls(), QStringList({"https://example.test/slow.csv"}));
}

QTEST_GUILESS_MAIN(CatalogDownloadServiceTests)

#include "CatalogDownloadServiceTests.moc"
