#include "TestHelpers.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

class CatalogRowsTests final : public QObject {
    Q_OBJECT

private slots:
    void bundledCatalogContainsExpectedBodies();
    void parserSkipsInvalidRowsAndParsesCoordinates();
    void parserRejectsEmptyAndInvalidInput();
};

void CatalogRowsTests::bundledCatalogContainsExpectedBodies()
{
    const auto catalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 27U);

    using namespace skygate::ephemeris::tests;
    const auto* sun = findBodyById(bodies, "sun");
    const auto* sirius = findBodyById(bodies, "sirius");
    const auto* orion = findBodyById(bodies, "orion");
    const auto* uranus = findBodyById(bodies, "uranus");
    const auto* neptune = findBodyById(bodies, "neptune");

    QVERIFY(sun != nullptr);
    QVERIFY(sun->type == skygate::ephemeris::CelestialBodyType::Sun);
    QVERIFY(sirius != nullptr);
    QVERIFY(sirius->type == skygate::ephemeris::CelestialBodyType::Star);
    QVERIFY(orion != nullptr);
    QVERIFY(orion->type == skygate::ephemeris::CelestialBodyType::Constellation);
    QVERIFY(uranus != nullptr);
    QVERIFY(uranus->type == skygate::ephemeris::CelestialBodyType::Planet);
    QVERIFY(neptune != nullptr);
    QVERIFY(neptune->type == skygate::ephemeris::CelestialBodyType::Planet);
}

void CatalogRowsTests::parserSkipsInvalidRowsAndParsesCoordinates()
{
    const auto catalog = skygate::ephemeris::createStarCatalogFromRows(
        "# comment row\n"
        "short|TooShort|Star\n"
        "|NoId|Star|1.0\n"
        "nodisplay||Star|1.0\n"
        "badtype|BadType|Asteroid|1.0\n"
        "badmag|BadMag|Star|nan\n"
        "vega| Vega |Star|0.03|18.6156|38.7837\n"
        "partial|Partial|Star|2.0|1.2\n"
        "spaced_id | Spaced Name | Moon | -12.0\n"
    );
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 3U);

    using namespace skygate::ephemeris::tests;
    const auto* vega = findBodyById(bodies, "vega");
    QVERIFY(vega != nullptr);
    QVERIFY(vega->displayName == "Vega");
    QVERIFY(vega->fixedEquatorial.has_value());
    if (vega->fixedEquatorial.has_value()) {
        QVERIFY(isNear(vega->fixedEquatorial->rightAscensionHours, 18.6156, 1e-6));
        QVERIFY(isNear(vega->fixedEquatorial->declinationDeg, 38.7837, 1e-6));
    }

    const auto* partial = findBodyById(bodies, "partial");
    QVERIFY(partial != nullptr);
    QVERIFY(!partial->fixedEquatorial.has_value());

    const auto* spacedId = findBodyById(bodies, "spaced_id");
    QVERIFY(spacedId != nullptr);
    QVERIFY(spacedId->displayName == "Spaced Name");
    QVERIFY(spacedId->type == skygate::ephemeris::CelestialBodyType::Moon);
}

void CatalogRowsTests::parserRejectsEmptyAndInvalidInput()
{
    const auto emptyCatalog = skygate::ephemeris::createStarCatalogFromRows("");
    QVERIFY(emptyCatalog == nullptr);

    const auto invalidCatalog = skygate::ephemeris::createStarCatalogFromRows(
        "# only comments\n"
        "broken_row_without_pipes\n"
        "still|bad|row\n"
    );
    QVERIFY(invalidCatalog == nullptr);
}

QTEST_APPLESS_MAIN(CatalogRowsTests)

#include "CatalogRowsTests.moc"
