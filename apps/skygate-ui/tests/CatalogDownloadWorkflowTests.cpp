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
#include <utility>

namespace {

struct FakeNetworkResponse final {
    QByteArray payload;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString errorText;
    int httpStatusCode = 200;
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

        QTimer::singleShot(0, this, [this] {
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
    void downloadServiceRetriesUntilFirstSuccessfulPayload();
    void downloadServiceRejectsEmptyAndOversizedPayloads();
    void coordinatorParsesSuccessfulDownloadedCatalog();
    void coordinatorReportsParseFailureWithSourceUrl();
    void importWorkflowParsesConstellationPayloads();
    void importWorkflowFallsBackForMalformedConstellationPayload();
    void importWorkflowRejectsDeepSkyCatalogWithoutDsos();
    void importWorkflowReportsDeepSkyObjectCount();
};

void CatalogDownloadWorkflowTests::invalidUrlCompletesWithoutNetworkRequest()
{
    FakeNetworkAccessManager networkAccessManager;
    CatalogDownloadService service(&networkAccessManager);

    bool completed = false;
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
