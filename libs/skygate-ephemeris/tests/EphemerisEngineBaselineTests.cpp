#include "TestHelpers.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

}  // namespace

class EphemerisEngineBaselineTests final : public QObject {
    Q_OBJECT

private slots:
    void computesFiniteSolarSystemCoordinates();
    void movingBodiesChangeAcrossDaysWhileFixedStarsStayStable();
    void supportsNullCatalogAndImportedFixedCoordinates();
};

void EphemerisEngineBaselineTests::computesFiniteSolarSystemCoordinates()
{
    const auto catalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.observer.elevationMeters = 16.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto snapshot = engine->compute(context);
    QVERIFY(snapshot.states.size() == catalog->bodies().size());
    QVERIFY(snapshot.context.observer.latitudeDeg == context.observer.latitudeDeg);

    using namespace skygate::ephemeris::tests;
    const auto* sun = findStateById(snapshot, "sun");
    const auto* moon = findStateById(snapshot, "moon");
    const auto* mercury = findStateById(snapshot, "mercury");
    const auto* venus = findStateById(snapshot, "venus");
    const auto* mars = findStateById(snapshot, "mars");
    const auto* jupiter = findStateById(snapshot, "jupiter");
    const auto* saturn = findStateById(snapshot, "saturn");
    const auto* uranus = findStateById(snapshot, "uranus");
    const auto* neptune = findStateById(snapshot, "neptune");
    const auto* sirius = findStateById(snapshot, "sirius");

    QVERIFY(sun != nullptr);
    QVERIFY(sun->equatorial.rightAscensionHours > 0.0 && sun->equatorial.rightAscensionHours < 24.0);
    QVERIFY(std::abs(sun->equatorial.declinationDeg) <= 90.0);
    QVERIFY(std::abs(sun->horizontal.altitudeDeg) <= 90.0);
    QVERIFY(sun->horizontal.azimuthDeg >= 0.0 && sun->horizontal.azimuthDeg < 360.0);

    QVERIFY(moon != nullptr);
    QVERIFY(moon->equatorial.rightAscensionHours > 0.0 && moon->equatorial.rightAscensionHours < 24.0);
    QVERIFY(std::abs(moon->equatorial.declinationDeg) <= 90.0);
    QVERIFY(std::abs(moon->horizontal.altitudeDeg) <= 90.0);
    QVERIFY(moon->horizontal.azimuthDeg >= 0.0 && moon->horizontal.azimuthDeg < 360.0);

    QVERIFY(mercury != nullptr);
    QVERIFY(std::isfinite(mercury->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(mercury->equatorial.declinationDeg));

    QVERIFY(venus != nullptr);
    QVERIFY(std::isfinite(venus->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(venus->equatorial.declinationDeg));

    QVERIFY(mars != nullptr);
    QVERIFY(std::isfinite(mars->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(mars->equatorial.declinationDeg));

    QVERIFY(jupiter != nullptr);
    QVERIFY(std::isfinite(jupiter->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(jupiter->equatorial.declinationDeg));

    QVERIFY(saturn != nullptr);
    QVERIFY(std::isfinite(saturn->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(saturn->equatorial.declinationDeg));

    QVERIFY(uranus != nullptr);
    QVERIFY(std::isfinite(uranus->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(uranus->equatorial.declinationDeg));

    QVERIFY(neptune != nullptr);
    QVERIFY(std::isfinite(neptune->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(neptune->equatorial.declinationDeg));

    QVERIFY(sirius != nullptr);
    QVERIFY(isNear(sirius->equatorial.rightAscensionHours, 6.7525, 1e-4));
    QVERIFY(isNear(sirius->equatorial.declinationDeg, -16.7161, 1e-4));
}

void EphemerisEngineBaselineTests::movingBodiesChangeAcrossDaysWhileFixedStarsStayStable()
{
    const auto catalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto baselineSnapshot = engine->compute(context);
    context.utcTime += std::chrono::seconds(86400);
    const auto nextDaySnapshot = engine->compute(context);

    using namespace skygate::ephemeris::tests;
    const auto* sun = findStateById(baselineSnapshot, "sun");
    const auto* moon = findStateById(baselineSnapshot, "moon");
    const auto* sirius = findStateById(baselineSnapshot, "sirius");
    const auto* nextSun = findStateById(nextDaySnapshot, "sun");
    const auto* nextMoon = findStateById(nextDaySnapshot, "moon");
    const auto* nextSirius = findStateById(nextDaySnapshot, "sirius");

    QVERIFY(sun != nullptr);
    QVERIFY(nextSun != nullptr);
    QVERIFY(std::abs(sun->equatorial.rightAscensionHours - nextSun->equatorial.rightAscensionHours) > 1e-5);

    QVERIFY(moon != nullptr);
    QVERIFY(nextMoon != nullptr);
    QVERIFY(std::abs(moon->equatorial.rightAscensionHours - nextMoon->equatorial.rightAscensionHours) > 1e-4);

    QVERIFY(sirius != nullptr);
    QVERIFY(nextSirius != nullptr);
    QVERIFY(isNear(
        sirius->equatorial.rightAscensionHours,
        nextSirius->equatorial.rightAscensionHours,
        1e-8
    ));
    QVERIFY(isNear(
        sirius->equatorial.declinationDeg,
        nextSirius->equatorial.declinationDeg,
        1e-8
    ));
}

void EphemerisEngineBaselineTests::supportsNullCatalogAndImportedFixedCoordinates()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto nullCatalogEngine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(nullCatalogEngine != nullptr);
    QVERIFY(nullCatalogEngine->compute(context).states.empty());

    const auto importedCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0
            }
        ),
    });
    QVERIFY(importedCatalog != nullptr);

    const auto importedEngine = skygate::ephemeris::createEphemerisEngine(*importedCatalog);
    QVERIFY(importedEngine != nullptr);
    const auto importedSnapshot = importedEngine->compute(context);
    QVERIFY(importedSnapshot.states.size() == 1U);
    QVERIFY(skygate::ephemeris::tests::isNear(
        importedSnapshot.states[0].equatorial.rightAscensionHours,
        12.5,
        1e-8
    ));
    QVERIFY(skygate::ephemeris::tests::isNear(
        importedSnapshot.states[0].equatorial.declinationDeg,
        -30.0,
        1e-8
    ));
}

QTEST_APPLESS_MAIN(EphemerisEngineBaselineTests)

#include "EphemerisEngineBaselineTests.moc"
