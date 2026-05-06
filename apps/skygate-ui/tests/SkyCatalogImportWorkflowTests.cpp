#include "AsyncTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "catalog/SkyCatalogImportWorkflow.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "LogCapture.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>

#include <algorithm>
#include <memory>
#include <utility>

using namespace skygate::ui::tests;

class SkyCatalogImportWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesConstellationPayloads();
    void doesNotCompleteConstellationAfterContextDestroyed();
    void fallsBackForMalformedConstellationPayload();
    void rejectsDeepSkyCatalogWithoutDsos();
    void reportsDeepSkyObjectCount();
};

void SkyCatalogImportWorkflowTests::parsesConstellationPayloads()
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

void SkyCatalogImportWorkflowTests::doesNotCompleteConstellationAfterContextDestroyed()
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

void SkyCatalogImportWorkflowTests::fallsBackForMalformedConstellationPayload()
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

void SkyCatalogImportWorkflowTests::rejectsDeepSkyCatalogWithoutDsos()
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

void SkyCatalogImportWorkflowTests::reportsDeepSkyObjectCount()
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

QTEST_GUILESS_MAIN(SkyCatalogImportWorkflowTests)

#include "SkyCatalogImportWorkflowTests.moc"
