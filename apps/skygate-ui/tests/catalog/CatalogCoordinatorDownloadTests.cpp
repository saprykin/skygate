#include "AsyncTestSupport.hpp"
#include "CatalogCoordinator.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using namespace skygate::ui::tests;

class CatalogCoordinatorDownloadTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesSuccessfulDownloadedCatalog();
    void reportsParseFailureWithSourceUrl();
};

void CatalogCoordinatorDownloadTests::parsesSuccessfulDownloadedCatalog()
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

void CatalogCoordinatorDownloadTests::reportsParseFailureWithSourceUrl()
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

QTEST_GUILESS_MAIN(CatalogCoordinatorDownloadTests)

#include "CatalogCoordinatorDownloadTests.moc"
