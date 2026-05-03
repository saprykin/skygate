#include "CatalogCoordinator.hpp"
#include "catalog/CatalogDownloadService.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"

#include <QtTest/QtTest>

#include <QEventLoop>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

namespace {

int severityRank(const QtMsgType type) noexcept
{
    switch (type) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }

    return 1;
}

struct FakeNetworkResponse final {
    QByteArray payload;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString errorText;
    int httpStatusCode = 200;
    int delayMs = 0;
};

class FakeNetworkReply final : public QNetworkReply {
    Q_OBJECT

public:
    FakeNetworkReply(const QNetworkRequest& request, FakeNetworkResponse response, QObject* parent)
        : QNetworkReply(parent)
        , m_payload(std::move(response.payload))
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.httpStatusCode);
        setHeader(QNetworkRequest::ContentLengthHeader, m_payload.size());
        if (response.error != QNetworkReply::NoError) {
            setError(response.error, response.errorText);
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);

        QTimer::singleShot(response.delayMs, this, [this] {
            if (!m_payload.isEmpty()) {
                emit readyRead();
            }
            emit finished();
        });
    }

    void abort() override {}

    qint64 bytesAvailable() const override
    {
        return static_cast<qint64>(m_payload.size() - m_offset) + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, const qint64 maxSize) override
    {
        if (m_offset >= m_payload.size()) {
            return -1;
        }

        const qint64 bytesToRead = std::min<qint64>(
            maxSize,
            static_cast<qint64>(m_payload.size() - m_offset)
        );
        std::memcpy(data, m_payload.constData() + m_offset, static_cast<std::size_t>(bytesToRead));
        m_offset += bytesToRead;
        return bytesToRead;
    }

private:
    QByteArray m_payload;
    qsizetype m_offset = 0;
};

class FakeNetworkAccessManager final : public QNetworkAccessManager {
    Q_OBJECT

public:
    void enqueueResponse(const QString& url, FakeNetworkResponse response)
    {
        m_responses[url].enqueue(std::move(response));
    }

    [[nodiscard]] QStringList requestedUrls() const
    {
        return m_requestedUrls;
    }

protected:
    QNetworkReply* createRequest(
        const Operation operation,
        const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr
    ) override
    {
        (void)operation;
        (void)outgoingData;

        const QString url = request.url().toString();
        m_requestedUrls.push_back(url);
        FakeNetworkResponse response;
        auto responseIt = m_responses.find(url);
        if (responseIt != m_responses.end() && !responseIt->isEmpty()) {
            response = responseIt->dequeue();
        } else {
            response.error = QNetworkReply::HostNotFoundError;
            response.errorText = "No fake response registered";
            response.httpStatusCode = 404;
        }
        return new FakeNetworkReply(request, std::move(response), this);
    }

private:
    QHash<QString, QQueue<FakeNetworkResponse>> m_responses;
    QStringList m_requestedUrls;
};

class LogCapture final {
public:
    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg)
        : m_minimumType(minimumType)
    {
        s_current = this;
        m_previousHandler = qInstallMessageHandler(&LogCapture::handler);
    }

    ~LogCapture()
    {
        qInstallMessageHandler(m_previousHandler);
        s_current = nullptr;
    }

    [[nodiscard]] QString joinedMessages() const
    {
        return m_messages.join('\n');
    }

private:
    static void handler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (
            s_current != nullptr
            && severityRank(type) >= severityRank(s_current->m_minimumType)
        ) {
            s_current->m_messages.push_back(QStringLiteral("%1 %2").arg(
                QString::fromUtf8(context.category != nullptr ? context.category : "default"),
                message
            ));
        }
    }

private:
    QtMessageHandler m_previousHandler = nullptr;
    QtMsgType m_minimumType = QtWarningMsg;
    QStringList m_messages;
    static inline LogCapture* s_current = nullptr;
};

template <typename StartFn>
void runAsync(StartFn startFn)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    startFn(loop);
    loop.exec();
    const bool timedOut = !timeout.isActive();
    timeout.stop();
    QVERIFY(!timedOut);
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,1,Alpha,1.0,2.0,3.0\n"}
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,1,Alpha,1.0,2.0,3.0\n"}
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
            .payload = "id,hip,proper,ra,dec,mag\n1,1,Slow,1.0,2.0,3.0\n",
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
    callbackContext.reset();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,1,Unexpected,1.0,2.0,3.0\n"}
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
    callbackContext.reset();
    QTest::qWait(60);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
    QCOMPARE(networkAccessManager.requestedUrls().size(), 1);
}

void CatalogDownloadWorkflowTests::downloadServiceCompletesConcurrentRequestsIndependently()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/first.csv",
        {
            .payload = "id,hip,proper,ra,dec,mag\n1,11,First,1.0,2.0,3.0\n",
            .delayMs = 25
        }
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/second.csv",
        {.payload = "id,hip,proper,ra,dec,mag\n2,22,Second,4.0,5.0,6.0\n"}
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,101,Alpha,1.0,2.0,3.0\n"}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/b-fail.csv",
        {.payload = QByteArray()}
    );
    networkAccessManager.enqueueResponse(
        "https://example.test/b-ok.csv",
        {
            .payload = "id,hip,proper,ra,dec,mag\n2,202,Beta,4.0,5.0,6.0\n",
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,42,Sirius,6.7525,-16.7161,-1.46\n"}
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,42,Bad,not-ra,2.0,3.0\n"}
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
        {.payload = R"({
            "constellations": [
                {
                    "id": "orion",
                    "common_name": { "english": "Orion" },
                    "lines": [[24436, 25930, 26727]]
                }
            ]
        })"}
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
            .payload = R"({
                "constellations": [
                    {
                        "id": "orion",
                        "common_name": { "english": "Orion" },
                        "lines": [[24436, 25930]]
                    }
                ]
            })",
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

    callbackContext.reset();
    QTest::qWait(60);
    QCoreApplication::processEvents();

    QVERIFY(!completionCalled);
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
        {.payload = "id,hip,proper,ra,dec,mag\n1,42,Sirius,6.7525,-16.7161,-1.46\n"}
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
        {.payload =
            "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;"
            "SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;"
            "Identifiers;Common names;NED notes;OpenNGC notes\n"
            "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;"
            "13.35;SA(s)b;;;;31;0224;;;PGC 2557,UGC 454;Andromeda Galaxy;;\n"}
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
