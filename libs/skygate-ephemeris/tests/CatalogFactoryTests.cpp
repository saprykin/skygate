#include "TestHelpers.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <algorithm>
#include <span>
#include <string>

class CatalogFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void createsBundledCatalogBySourceType();
    void createsHygCatalogBySourceRequest();
    void createsHygCatalogBySourceTypeWithProgress();
    void mergesDownloadedDeepSkyObjectsOverBundledAliases();
    void reportsDiagnosticsForSelectionAndErrors();
};

void CatalogFactoryTests::createsBundledCatalogBySourceType()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceType::Bundled
    );
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

void CatalogFactoryTests::createsHygCatalogBySourceRequest()
{
    const auto catalog = skygate::ephemeris::createStarCatalog(
        skygate::ephemeris::CatalogSourceRequest {
            .type = skygate::ephemeris::CatalogSourceType::HygCsv,
            .data =
                "id,hip,proper,ra,dec,mag\n"
                "1,11,Alpha,12.5,-30.0,4.0\n"
        }
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_11");
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

void CatalogFactoryTests::mergesDownloadedDeepSkyObjectsOverBundledAliases()
{
    skygate::ephemeris::CelestialBody bundledMessier;
    bundledMessier.id = "messier_031";
    bundledMessier.displayName = "M31";
    bundledMessier.type = skygate::ephemeris::CelestialBodyType::DeepSkyObject;
    bundledMessier.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 0.7,
        .declinationDeg = 41.0
    };
    bundledMessier.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"M31", "NGC 224"}
    };

    skygate::ephemeris::CelestialBody star;
    star.id = "hip_42";
    star.displayName = "Sirius";
    star.type = skygate::ephemeris::CelestialBodyType::Star;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 6.7525,
        .declinationDeg = -16.7161
    };

    skygate::ephemeris::CelestialBody downloadedMessier = bundledMessier;
    downloadedMessier.id = "open_ngc_m31";
    downloadedMessier.displayName = "OpenNGC M31";
    downloadedMessier.deepSkyObject->aliases = {"M 31", "NGC0224"};

    const auto mergedCatalog = skygate::ephemeris::createCatalogByMergingDeepSkyObjects(
        std::span<const skygate::ephemeris::CelestialBody> {&star, 1U},
        std::span<const skygate::ephemeris::CelestialBody> {&downloadedMessier, 1U}
    );
    const auto baseCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        star,
        bundledMessier
    });
    const auto downloadedCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        downloadedMessier
    });
    const auto mergedWithReplacement = skygate::ephemeris::createCatalogByMergingDeepSkyObjects(
        *baseCatalog,
        *downloadedCatalog
    );

    QVERIFY(mergedCatalog != nullptr);
    QVERIFY(mergedWithReplacement != nullptr);
    QCOMPARE(mergedCatalog->bodies().size(), 2U);
    QCOMPARE(mergedWithReplacement->bodies().size(), 2U);
    QVERIFY(std::any_of(
        mergedWithReplacement->bodies().begin(),
        mergedWithReplacement->bodies().end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.id == "open_ngc_m31";
        }
    ));
    QVERIFY(std::none_of(
        mergedWithReplacement->bodies().begin(),
        mergedWithReplacement->bodies().end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.id == "messier_031";
        }
    ));
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
