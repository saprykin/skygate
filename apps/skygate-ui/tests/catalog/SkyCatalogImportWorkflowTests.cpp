#include "AsyncTestSupport.hpp"
#include "BaseCelestialBody.hpp"
#include "CatalogArchiveTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "LogCapture.hpp"

#include "catalog/SkyCatalogImportWorkflow.hpp"
#include "catalog/SkyCatalogSourceDescriptor.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <utility>

using namespace skygate::ui::tests;

namespace {

// A gzip-compressed HYG CSV row; decodes to the single star object hip_42.
constexpr std::array<unsigned char, 73> kHygGzip{
    0x1f, 0x8b, 0x08, 0x00, 0x77, 0xa9, 0x86, 0x69, 0x00, 0x03, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29, 0x28,
    0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32, 0xd7, 0x31,
    0x31, 0xd2, 0x09, 0x49, 0x2d, 0x2e, 0x09, 0x2e, 0x49, 0x2c, 0xd2, 0x31, 0xd4, 0x33, 0x32, 0xd5, 0xd1, 0x35, 0xd2,
    0x33, 0xd5, 0x31, 0xd6, 0x33, 0xe2, 0x02, 0x00, 0xb9, 0xd5, 0xee, 0x71, 0x35, 0x00, 0x00, 0x00
};

QByteArray hygGzipPayload()
{
    return QByteArray(reinterpret_cast<const char*>(kHygGzip.data()), static_cast<qsizetype>(kHygGzip.size()));
}

QByteArray singleMemberCatalogZip()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = "catalog/hyg.csv",
            .data = sampleHygCsvPayload().toStdString(),
        },
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

skygate::ui::internal::SkyCatalogSourceInstance makeDescriptorInstance(
    const QString& url, const skygate::ephemeris::CatalogSourceType schemaHint, const QString& archiveSelector = {}
)
{
    skygate::ui::internal::SkyCatalogSourceDescriptor descriptor;
    descriptor.sourceId = QStringLiteral("demo_source");
    descriptor.title = QStringLiteral("Demo Source");
    descriptor.urls = QStringList{url};
    descriptor.schemaHint = schemaHint;
    descriptor.archiveSelector = archiveSelector;
    return skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(descriptor);
}

}  // namespace

class SkyCatalogImportWorkflowTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesConstellationPayloads();
    void doesNotCompleteConstellationAfterContextDestroyed();
    void fallsBackForMalformedConstellationPayload();
    void rejectsDeepSkyCatalogWithoutDsos();
    void reportsDeepSkyObjectCount();
    void unifiedDownloadSourceKeepsStarCatalogsUnfiltered();
    void loadsDescriptorSelectedMemberFromMultiMemberArchive();
    void reportsMissingDescriptorArchiveMember();
    void reportsContradictorySchemaHint();
    void reportsUnsupportedSchemaPayload();
    void reportsAmbiguousArchiveWithoutDescriptorSelection();
    void retainsExistingBehaviorForSingleMemberZipGzipAndPlain();

private:
    void runSourceDownload(
        FakeNetworkAccessManager& networkAccessManager,
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ui::internal::SkyCatalogSourceImportResult& finalResult
    );
};

void SkyCatalogImportWorkflowTests::runSourceDownload(
    FakeNetworkAccessManager& networkAccessManager,
    const skygate::ui::internal::SkyCatalogSourceInstance& source,
    skygate::ui::internal::SkyCatalogSourceImportResult& finalResult
)
{
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);
    runAsync([&](QEventLoop& loop) {
        workflow.downloadSource(
            source,
            skygate::ephemeris::CatalogCompositionPolicy::Merge,
            this,
            {},
            [&finalResult, &loop](skygate::ui::internal::SkyCatalogSourceImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });
}

void SkyCatalogImportWorkflowTests::parsesConstellationPayloads()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/index.json", {.payload = sampleConstellationIndexJsonPayload()}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyConstellationLineImportResult finalResult;
    QStringList statuses;
    LogCapture capture(QtInfoMsg);
    runAsync([&](QEventLoop& loop) {
        workflow.downloadConstellationLines(
            {"https://example.test/index.json"},
            this,
            [&statuses](const QString& status) { statuses.push_back(status); },
            [&finalResult, &loop](skygate::ui::internal::SkyConstellationLineImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QCOMPARE(finalResult.constellationCount, 1U);
    QCOMPARE(finalResult.lineRefs.size(), 2U);
    QCOMPARE(finalResult.anchorGroups.size(), 1U);
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
        "https://example.test/slow-index.json", {.payload = sampleConstellationIndexJsonPayload(), .delayMs = 20}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    bool completionCalled = false;
    auto callbackContext = std::make_unique<QObject>();
    workflow.downloadConstellationLines(
        {"https://example.test/slow-index.json"},
        callbackContext.get(),
        {},
        [&completionCalled](skygate::ui::internal::SkyConstellationLineImportResult) { completionCalled = true; }
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
    networkAccessManager.enqueueResponse("https://example.test/bad-index.json", {.payload = "not-json"});
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyConstellationLineImportResult finalResult;
    QTest::ignoreMessage(
        QtWarningMsg, "Constellation line parse failed; no bundled fallback. Payload preview: not-json"
    );
    runAsync([&](QEventLoop& loop) {
        workflow.downloadConstellationLines(
            {"https://example.test/bad-index.json"},
            this,
            {},
            [&finalResult, &loop](skygate::ui::internal::SkyConstellationLineImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(!finalResult.hasCustomLines());
    QVERIFY(finalResult.anchorGroups.empty());
    QVERIFY(finalResult.statusSuffix.contains("no constellation data"));
    QVERIFY(finalResult.statusSuffix.contains("parse failed"));
    QVERIFY(finalResult.statusSuffix.contains("not-json"));
}

void SkyCatalogImportWorkflowTests::rejectsDeepSkyCatalogWithoutDsos()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/stars.csv", {.payload = sampleHygCsvPayload()});
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source;
    source.instanceId = QStringLiteral("preset:hyg");
    source.title = QStringLiteral("HYG");
    source.version = QStringLiteral("v4.2");
    source.urls = QStringList{QStringLiteral("https://example.test/stars.csv")};

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runAsync([&](QEventLoop& loop) {
        workflow.downloadSource(
            source,
            skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
            this,
            {},
            [&finalResult, &loop](skygate::ui::internal::SkyCatalogSourceImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QCOMPARE(finalResult.sourceLabel, QString("HYG"));
    QCOMPARE(finalResult.sourceId, QString("preset:hyg"));
    QCOMPARE(finalResult.sourceVersion, QString("v4.2"));
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/stars.csv"));
    QCOMPARE(finalResult.foundObjectCount, std::size_t{0});
    QCOMPARE(finalResult.errorText, QString("Catalog: Downloaded deep-sky catalog contains no DSOs"));
}

void SkyCatalogImportWorkflowTests::reportsDeepSkyObjectCount()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/open-ngc.csv", {.payload = sampleOpenNgcCsvPayload()});
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source;
    source.instanceId = QStringLiteral("preset:open_ngc");
    source.title = QStringLiteral("OpenNGC");
    source.version = QStringLiteral("v20260307");
    source.urls = QStringList{QStringLiteral("https://example.test/open-ngc.csv")};

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runAsync([&](QEventLoop& loop) {
        workflow.downloadSource(
            source,
            skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
            this,
            {},
            [&finalResult, &loop](skygate::ui::internal::SkyCatalogSourceImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    QCOMPARE(finalResult.sourceLabel, QString("OpenNGC"));
    QCOMPARE(finalResult.sourceId, QString("preset:open_ngc"));
    QCOMPARE(finalResult.sourceVersion, QString("v20260307"));
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/open-ngc.csv"));
    QCOMPARE(finalResult.foundObjectCount, 1U);
    QVERIFY(finalResult.errorText.isEmpty());
}

void SkyCatalogImportWorkflowTests::unifiedDownloadSourceKeepsStarCatalogsUnfiltered()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/stars.csv",
        {.payload = sampleHygCsvPayload({.hip = 1234, .properName = "Unified Star", .mag = "1.0"})}
    );
    const skygate::ui::internal::SkyCatalogImportWorkflow workflow(&networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source;
    source.instanceId = QStringLiteral("custom:stars");
    source.title = QStringLiteral("Unified Stars");
    source.version = QStringLiteral("v1");
    source.urls = QStringList{QStringLiteral("https://example.test/stars.csv")};

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runAsync([&](QEventLoop& loop) {
        workflow.downloadSource(
            source,
            skygate::ephemeris::CatalogCompositionPolicy::Merge,
            this,
            {},
            [&finalResult, &loop](skygate::ui::internal::SkyCatalogSourceImportResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    QCOMPARE(finalResult.foundObjectCount, std::size_t{0});
    QCOMPARE(finalResult.sourceLabel, QString("Unified Stars"));
    QCOMPARE(finalResult.sourceId, QString("custom:stars"));
    QCOMPARE(finalResult.sourceVersion, QString("v1"));
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/stars.csv"));
    QVERIFY(finalResult.errorText.isEmpty());
}

void SkyCatalogImportWorkflowTests::loadsDescriptorSelectedMemberFromMultiMemberArchive()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/catalogs.zip", {.payload = sampleTwoMemberCatalogZip()});

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runSourceDownload(
        networkAccessManager,
        makeDescriptorInstance(
            QStringLiteral("https://example.test/catalogs.zip"),
            skygate::ephemeris::CatalogSourceType::HygCsv,
            QStringLiteral("catalog/hyg.csv")
        ),
        finalResult
    );

    QVERIFY(finalResult.catalog != nullptr);
    const auto bodies = finalResult.catalog->bodies();
    QCOMPARE(bodies.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("hip_42"));
    QVERIFY(finalResult.errorText.isEmpty());
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/catalogs.zip"));
}

void SkyCatalogImportWorkflowTests::reportsMissingDescriptorArchiveMember()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/catalogs.zip", {.payload = sampleTwoMemberCatalogZip()});

    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload does not contain member 'catalog/missing.csv'."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/catalogs.zip parse failed: requested archive member not found (ZIP "
        "catalog payload does not contain member 'catalog/missing.csv'.)"
    );

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runSourceDownload(
        networkAccessManager,
        makeDescriptorInstance(
            QStringLiteral("https://example.test/catalogs.zip"),
            skygate::ephemeris::CatalogSourceType::HygCsv,
            QStringLiteral("catalog/missing.csv")
        ),
        finalResult
    );

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("requested archive member not found"));
    QVERIFY(finalResult.errorText.contains("catalog/missing.csv"));
}

void SkyCatalogImportWorkflowTests::reportsContradictorySchemaHint()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/stars.csv", {.payload = sampleHygCsvPayload()});

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload schema 'HYG CSV' does not match the expected schema hint "
        "'OpenNGC CSV'."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/stars.csv parse failed: schema hint mismatch (Catalog payload schema "
        "'HYG CSV' does not match the expected schema hint 'OpenNGC CSV'.)"
    );

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runSourceDownload(
        networkAccessManager,
        makeDescriptorInstance(
            QStringLiteral("https://example.test/stars.csv"), skygate::ephemeris::CatalogSourceType::OpenNgcCsv
        ),
        finalResult
    );

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("schema hint mismatch"));
    QVERIFY(finalResult.errorText.contains("OpenNGC CSV"));
}

void SkyCatalogImportWorkflowTests::reportsUnsupportedSchemaPayload()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/unknown.csv", {.payload = "not a catalog"});

    QTest::ignoreMessage(QtWarningMsg, "Catalog payload parse failed: Catalog payload format is not recognized.");
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/unknown.csv parse failed: unsupported format (Catalog payload format is "
        "not recognized.)"
    );

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runSourceDownload(
        networkAccessManager,
        makeDescriptorInstance(
            QStringLiteral("https://example.test/unknown.csv"), skygate::ephemeris::CatalogSourceType::HygCsv
        ),
        finalResult
    );

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("unsupported format"));
}

void SkyCatalogImportWorkflowTests::reportsAmbiguousArchiveWithoutDescriptorSelection()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/catalogs.zip", {.payload = sampleTwoMemberCatalogZip()});

    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload contains multiple supported catalog members."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/catalogs.zip parse failed: multiple supported archive members (ZIP "
        "catalog payload contains multiple supported catalog members.)"
    );

    skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
    runSourceDownload(
        networkAccessManager,
        makeDescriptorInstance(
            QStringLiteral("https://example.test/catalogs.zip"), skygate::ephemeris::CatalogSourceType::Unknown
        ),
        finalResult
    );

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("multiple supported archive members"));
}

void SkyCatalogImportWorkflowTests::retainsExistingBehaviorForSingleMemberZipGzipAndPlain()
{
    {
        FakeNetworkAccessManager networkAccessManager;
        networkAccessManager.enqueueResponse("https://example.test/one.zip", {.payload = singleMemberCatalogZip()});

        skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
        runSourceDownload(
            networkAccessManager,
            makeDescriptorInstance(
                QStringLiteral("https://example.test/one.zip"), skygate::ephemeris::CatalogSourceType::HygCsv
            ),
            finalResult
        );

        QVERIFY(finalResult.catalog != nullptr);
        const auto bodies = finalResult.catalog->bodies();
        QCOMPARE(bodies.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("hip_42"));
    }

    {
        FakeNetworkAccessManager networkAccessManager;
        networkAccessManager.enqueueResponse("https://example.test/stars.csv.gz", {.payload = hygGzipPayload()});

        skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
        runSourceDownload(
            networkAccessManager,
            makeDescriptorInstance(
                QStringLiteral("https://example.test/stars.csv.gz"), skygate::ephemeris::CatalogSourceType::HygCsv
            ),
            finalResult
        );

        QVERIFY(finalResult.catalog != nullptr);
        const auto bodies = finalResult.catalog->bodies();
        QCOMPARE(bodies.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("hip_42"));
    }

    {
        FakeNetworkAccessManager networkAccessManager;
        networkAccessManager.enqueueResponse("https://example.test/stars.csv", {.payload = sampleHygCsvPayload()});

        skygate::ui::internal::SkyCatalogSourceImportResult finalResult;
        runSourceDownload(
            networkAccessManager,
            makeDescriptorInstance(
                QStringLiteral("https://example.test/stars.csv"), skygate::ephemeris::CatalogSourceType::HygCsv
            ),
            finalResult
        );

        QVERIFY(finalResult.catalog != nullptr);
        const auto bodies = finalResult.catalog->bodies();
        QCOMPARE(bodies.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("hip_42"));
    }
}

QTEST_GUILESS_MAIN(SkyCatalogImportWorkflowTests)

#include "SkyCatalogImportWorkflowTests.moc"
