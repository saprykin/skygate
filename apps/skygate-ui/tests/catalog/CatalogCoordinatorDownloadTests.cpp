#include "AsyncTestSupport.hpp"
#include "BaseCelestialBody.hpp"
#include "CatalogCoordinator.hpp"
#include "CatalogParseOptions.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

using namespace skygate::ui::tests;

class CatalogCoordinatorDownloadTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesSuccessfulDownloadedCatalog();
    void reportsParseFailureWithSourceUrl();
    void gzipWithUnknownInnerSchemaUsesFormatNeutralWording();
    void missingOpenNgcColumnsDoNotClaimHyg();
    void loadsArchiveMemberNamedByParseOptions();
    void reportsContradictorySchemaHintWithSourceUrl();
};

void CatalogCoordinatorDownloadTests::parsesSuccessfulDownloadedCatalog()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/catalog.csv", {.payload = sampleHygCsvPayload()});
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QStringList statuses;
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/catalog.csv"},
            CatalogParseOptions{},
            this,
            [&statuses](const QString& status) { statuses.push_back(status); },
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    QCOMPARE(finalResult.diagnostics.parsedBodyCount, 1U);
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/catalog.csv"));
    QVERIFY(finalResult.errorText.isEmpty());
    QVERIFY(std::any_of(statuses.begin(), statuses.end(), [](const QString& status) {
        return status.startsWith("Catalog: Processing");
    }));
}

void CatalogCoordinatorDownloadTests::reportsParseFailureWithSourceUrl()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/bad.csv", {.payload = sampleHygCsvPayload({1, 42, "Bad", "not-ra", "2.0", "3.0"})}
    );
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QStringList statuses;
    QTest::ignoreMessage(
        QtWarningMsg,
        "HYG CSV skipped 1 rows with invalid numeric values; samples: row 2 ra='not-ra' dec='2.0' mag='3.0'"
    );
    QTest::ignoreMessage(QtWarningMsg, "HYG CSV parse failed: HYG CSV payload does not contain any valid star rows.");
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/bad.csv parse failed: invalid HYG CSV payload (HYG CSV payload does not "
        "contain any valid star rows.)"
    );
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/bad.csv"},
            CatalogParseOptions{},
            this,
            [&statuses](const QString& status) { statuses.push_back(status); },
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

void CatalogCoordinatorDownloadTests::gzipWithUnknownInnerSchemaUsesFormatNeutralWording()
{
    // A gzip-compressed CSV that is missing the required HYG "mag" column, so
    // the decoded inner payload is not recognized as any supported schema.
    constexpr std::array<unsigned char, 70> kHygGzipMissingMag{
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29,
        0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xe6, 0x32, 0xd4, 0x31, 0x31, 0xd2,
        0x09, 0x49, 0x2d, 0x2e, 0x51, 0x08, 0x2e, 0x49, 0x2c, 0xd2, 0x31, 0xd3, 0x33, 0x37, 0x35, 0x32, 0xd5, 0xd1,
        0x35, 0x04, 0x32, 0x0c, 0xcd, 0x0c, 0xb9, 0x00, 0x65, 0xc5, 0x50, 0x38, 0x34, 0x00, 0x00, 0x00,
    };

    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        "https://example.test/missing-mag.csv.gz",
        {.payload = QByteArray(
             reinterpret_cast<const char*>(kHygGzipMissingMag.data()), static_cast<qsizetype>(kHygGzipMissingMag.size())
         )}
    );
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog container decoded, but the inner payload is not a recognized catalog "
        "format."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/missing-mag.csv.gz parse failed: unsupported format (Catalog container "
        "decoded, but the inner payload is not a recognized catalog format.)"
    );
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/missing-mag.csv.gz"},
            CatalogParseOptions{},
            this,
            {},
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("unsupported format"));
    QVERIFY(!finalResult.errorText.contains("HYG"));
}

void CatalogCoordinatorDownloadTests::missingOpenNgcColumnsDoNotClaimHyg()
{
    const QByteArray payload = "Name;Type;RA\nNGC0224;G;00:42:44.35\n";
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/missing-dec.csv", {.payload = payload});
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    QTest::ignoreMessage(QtWarningMsg, "Catalog payload parse failed: Catalog payload format is not recognized.");
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog: Source https://example.test/missing-dec.csv parse failed: unsupported format (Catalog payload format "
        "is not recognized.)"
    );
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/missing-dec.csv"},
            CatalogParseOptions{},
            this,
            {},
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("unsupported format"));
    QVERIFY(!finalResult.errorText.contains("HYG"));
    QVERIFY(!finalResult.errorText.contains("OpenNGC"));
}

void CatalogCoordinatorDownloadTests::loadsArchiveMemberNamedByParseOptions()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/catalogs.zip", {.payload = sampleTwoMemberCatalogZip()});
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/catalogs.zip"},
            CatalogParseOptions{
                .archiveMember = QStringLiteral("catalog/ngc.csv"),
                .schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv,
            },
            this,
            {},
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog != nullptr);
    const auto bodies = finalResult.catalog->bodies();
    QCOMPARE(bodies.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("messier_031"));
    QVERIFY(finalResult.errorText.isEmpty());
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/catalogs.zip"));
}

void CatalogCoordinatorDownloadTests::reportsContradictorySchemaHintWithSourceUrl()
{
    FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse("https://example.test/stars.csv", {.payload = sampleHygCsvPayload()});
    CatalogCoordinator coordinator(&networkAccessManager);

    CatalogCoordinator::DownloadResult finalResult;
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
    runAsync([&](QEventLoop& loop) {
        coordinator.downloadCatalogFromUrls(
            {"https://example.test/stars.csv"},
            CatalogParseOptions{.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv},
            this,
            {},
            [&finalResult, &loop](CatalogCoordinator::DownloadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.catalog == nullptr);
    QVERIFY(finalResult.errorText.contains("schema hint mismatch"));
    QVERIFY(finalResult.errorText.contains("HYG CSV"));
    QVERIFY(finalResult.errorText.contains("OpenNGC CSV"));
    QCOMPARE(finalResult.sourceUrl, QString("https://example.test/stars.csv"));
}

QTEST_GUILESS_MAIN(CatalogCoordinatorDownloadTests)

#include "CatalogCoordinatorDownloadTests.moc"
