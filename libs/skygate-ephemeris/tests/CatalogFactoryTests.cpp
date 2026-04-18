#include "TestHelpers.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <cstddef>

class CatalogFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void createsBundledCatalogBySourceType();
    void createsRowsCatalogBySourceRequest();
    void createsHygCatalogBySourceTypeWithProgress();
    void reportsDiagnosticsForSelectionAndErrors();
};

void CatalogFactoryTests::createsBundledCatalogBySourceType()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceType::Bundled
    );
    QVERIFY(catalog != nullptr);
    QVERIFY(catalog->bodies().size() == 27U);
}

void CatalogFactoryTests::createsRowsCatalogBySourceRequest()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceRequest {
            .type = skygate::ephemeris::CatalogSourceType::Rows,
            .data = "demo_star|Demo Star|Star|4.0|12.5|-30.0\n"
        }
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "demo_star");
    QVERIFY(bodies[0].fixedEquatorial.has_value());
}

void CatalogFactoryTests::createsHygCatalogBySourceTypeWithProgress()
{
    std::size_t callbackLastCount = 0;
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        "1,11,Alpha,1.0,2.0,3.0\n"
        "2,22,Beta,4.0,5.0,6.0\n",
        [&callbackLastCount](const std::size_t parsedObjectCount) {
            callbackLastCount = parsedObjectCount;
        }
    );
    QVERIFY(catalog != nullptr);
    QVERIFY(catalog->bodies().size() == 2U);
    QVERIFY(callbackLastCount == 2U);
}

void CatalogFactoryTests::reportsDiagnosticsForSelectionAndErrors()
{
    const auto selectedCatalog = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        "1,11,Alpha,1.0,2.0,3.0\n"
        "2,22,Beta,4.0,5.0,6.0\n",
        {},
        skygate::ephemeris::CatalogSelectionOptions {
            .mode = skygate::ephemeris::CatalogSelectionMode::BrightestByVisualMagnitude,
            .maxBodyCount = 1U
        }
    );
    QVERIFY(selectedCatalog.isSuccess());
    QVERIFY(selectedCatalog.diagnostics.parsedBodyCount == 2U);
    QVERIFY(selectedCatalog.diagnostics.selectedBodyCount == 1U);
    QVERIFY(selectedCatalog.diagnostics.truncatedBodyCount == 1U);

    const auto invalidCatalog = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,name\n"
        "1,NoCoordinates\n"
    );
    QVERIFY(!invalidCatalog.isSuccess());
    QVERIFY(
        invalidCatalog.errorCode == skygate::ephemeris::CatalogLoadErrorCode::MissingRequiredColumns
    );
}

QTEST_APPLESS_MAIN(CatalogFactoryTests)

#include "CatalogFactoryTests.moc"
