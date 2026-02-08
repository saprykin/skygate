#include "TestHelpers.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>

class EphemerisEngineFallbackTests final : public QObject {
    Q_OBJECT

private slots:
    void usesFallbackBodyLookupAndFixedCoordinatePriority();
    void skipsHorizontalCoordinatesForInvalidObserver();
};

void EphemerisEngineFallbackTests::usesFallbackBodyLookupAndFixedCoordinatePriority()
{
    const auto catalog = skygate::ephemeris::createStarCatalogFromRows(
        "sun|SunById|Star|0.0\n"
        "moon|MoonById|Planet|0.0\n"
        "fixed_sun|Fixed Sun|Sun|0.0|1.5|2.5\n"
        "unknown_star|Unknown Star|Star|0.0\n"
        "planet_x|Planet X|Planet|0.0\n"
        "orion|Orion|Constellation|1.0\n"
        "unknown_constellation|Unknown Constellation|Constellation|1.0\n"
    );
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(catalog.get());
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 10.0;
    context.observer.longitudeDeg = 20.0;
    context.observer.elevationMeters = 5.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1710000000));

    const auto snapshot = engine->compute(context);
    QVERIFY(snapshot.states.size() == 7U);
    QVERIFY(snapshot.context.utcTime.time_since_epoch() == context.utcTime.time_since_epoch());

    using namespace skygate::ephemeris::tests;
    const auto* sunById = findStateById(snapshot, "sun");
    const auto* moonById = findStateById(snapshot, "moon");
    const auto* fixedSun = findStateById(snapshot, "fixed_sun");
    const auto* unknownStar = findStateById(snapshot, "unknown_star");
    const auto* planetX = findStateById(snapshot, "planet_x");
    const auto* orion = findStateById(snapshot, "orion");
    const auto* unknownConstellation = findStateById(snapshot, "unknown_constellation");

    QVERIFY(sunById != nullptr);
    QVERIFY(std::isfinite(sunById->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(sunById->equatorial.declinationDeg));

    QVERIFY(moonById != nullptr);
    QVERIFY(std::isfinite(moonById->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(moonById->equatorial.declinationDeg));

    QVERIFY(fixedSun != nullptr);
    QVERIFY(isNear(fixedSun->equatorial.rightAscensionHours, 1.5, 1e-8));
    QVERIFY(isNear(fixedSun->equatorial.declinationDeg, 2.5, 1e-8));

    QVERIFY(unknownStar != nullptr);
    QVERIFY(std::isnan(unknownStar->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(unknownStar->equatorial.declinationDeg));

    QVERIFY(planetX != nullptr);
    QVERIFY(std::isnan(planetX->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(planetX->equatorial.declinationDeg));

    QVERIFY(orion != nullptr);
    QVERIFY(isNear(orion->equatorial.rightAscensionHours, 5.5833, 1e-4));
    QVERIFY(isNear(orion->equatorial.declinationDeg, 5.0, 1e-4));

    QVERIFY(unknownConstellation != nullptr);
    QVERIFY(std::isnan(unknownConstellation->equatorial.rightAscensionHours));
}

void EphemerisEngineFallbackTests::skipsHorizontalCoordinatesForInvalidObserver()
{
    const auto catalog = skygate::ephemeris::createStarCatalogFromRows(
        "sun|Sun|Sun|0.0\n"
    );
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(catalog.get());
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 120.0;
    context.observer.longitudeDeg = 10.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1710000000));

    const auto snapshot = engine->compute(context);
    const auto* sun = skygate::ephemeris::tests::findStateById(snapshot, "sun");
    QVERIFY(sun != nullptr);
    QVERIFY(std::isfinite(sun->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(sun->horizontal.altitudeDeg));
    QVERIFY(std::isnan(sun->horizontal.azimuthDeg));
}

QTEST_APPLESS_MAIN(EphemerisEngineFallbackTests)

#include "EphemerisEngineFallbackTests.moc"
