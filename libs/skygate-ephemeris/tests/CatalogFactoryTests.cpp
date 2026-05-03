#include "TestHelpers.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <algorithm>
#include <span>
#include <string>

class CatalogFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledCatalogBySourceType();
    void loadsHygCatalogBySourceRequest();
    void loadsHygCatalogBySourceTypeWithProgress();
    void reportsDiagnosticsForSelectionAndErrors();
};

void CatalogFactoryTests::loadsBundledCatalogBySourceType()
{
    auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::Bundled
    );
    QVERIFY(result.isSuccess());
    const auto& catalog = result.catalog;
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 119U);
    const auto messier31It = std::find_if(bodies.begin(), bodies.end(), [](const auto& body) {
        return body.id == "messier_031";
    });
    QVERIFY(messier31It != bodies.end());
    QVERIFY(messier31It->displayName == "M31");
    QVERIFY(messier31It->type == skygate::ephemeris::CelestialBodyType::DeepSkyObject);
    QVERIFY(messier31It->fixedEquatorial.has_value());
    QVERIFY(messier31It->deepSkyObject.has_value());
    QVERIFY(messier31It->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::Galaxy);
}

void CatalogFactoryTests::loadsHygCatalogBySourceRequest()
{
    auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceRequest {
            .type = skygate::ephemeris::CatalogSourceType::HygCsv,
            .data =
                "id,hip,proper,ra,dec,mag\n"
                "1,11,Alpha,12.5,-30.0,4.0\n"
        }
    );
    QVERIFY(result.isSuccess());
    const auto& catalog = result.catalog;
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_11");
    QVERIFY(bodies[0].fixedEquatorial.has_value());
}

void CatalogFactoryTests::loadsHygCatalogBySourceTypeWithProgress()
{
    std::size_t callbackLastCount = 0;
    auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        "1,11,Alpha,1.0,2.0,3.0\n"
        "2,22,Beta,4.0,5.0,6.0\n",
        [&callbackLastCount](const std::size_t parsedObjectCount) {
            callbackLastCount = parsedObjectCount;
        }
    );
    QVERIFY(result.isSuccess());
    const auto& catalog = result.catalog;
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
