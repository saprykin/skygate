#include "TestHelpers.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

class SnapshotOnlyEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        auto bodies = std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(
            std::vector<skygate::ephemeris::CelestialBody> {
                makeBody(
                    "HIP_42",
                    "HIP 42",
                    skygate::ephemeris::CelestialBodyType::Star,
                    1.0,
                    skygate::core::EquatorialCoordinate {
                        .rightAscensionHours = 2.0,
                        .declinationDeg = 3.0
                    }
                )
            }
        );

        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = std::move(bodies);
        snapshot.states.push_back(
            skygate::ephemeris::CelestialBodyState {
                .bodyIndex = 0,
                .equatorial = {
                    .rightAscensionHours = 2.0,
                    .declinationDeg = 3.0
                },
                .horizontal = {
                    .altitudeDeg = 4.0,
                    .azimuthDeg = 5.0
                }
            }
        );
        return snapshot;
    }
};

}  // namespace

class EphemerisEngineFallbackTests final : public QObject {
    Q_OBJECT

private slots:
    void usesFallbackBodyLookupAndFixedCoordinatePriority();
    void usesInterfaceDefaultBodyLookupCaseInsensitive();
    void usesInterfaceDefaultBodyLookupByIndex();
    void skipsHorizontalCoordinatesForInvalidObserver();
    void fixedCoordinatesOverrideExplicitSourceDispatch();
};

void EphemerisEngineFallbackTests::usesFallbackBodyLookupAndFixedCoordinatePriority()
{
    const auto catalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("sun", "SunById", skygate::ephemeris::CelestialBodyType::Star, 0.0),
        makeBody("moon", "MoonById", skygate::ephemeris::CelestialBodyType::Planet, 0.0),
        makeBody(
            "fixed_sun",
            "Fixed Sun",
            skygate::ephemeris::CelestialBodyType::Sun,
            0.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
        makeBody("unknown_star", "Unknown Star", skygate::ephemeris::CelestialBodyType::Star, 0.0),
        makeBody("planet_x", "Planet X", skygate::ephemeris::CelestialBodyType::Planet, 0.0),
        makeBody(
            "orion",
            "Orion",
            skygate::ephemeris::CelestialBodyType::Constellation,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5833,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "unknown_constellation",
            "Unknown Constellation",
            skygate::ephemeris::CelestialBodyType::Constellation,
            1.0
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
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
    const auto bodies = snapshot.bodies();

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
    QCOMPARE(
        snapshot.bodyAt(unknownStar->bodyIndex).ephemerisSource,
        skygate::ephemeris::CelestialBodyEphemerisSource::Unresolved
    );
    QVERIFY(std::isnan(unknownStar->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(unknownStar->equatorial.declinationDeg));

    QVERIFY(planetX != nullptr);
    QVERIFY(std::isnan(planetX->equatorial.rightAscensionHours));
    QVERIFY(std::isnan(planetX->equatorial.declinationDeg));

    QVERIFY(orion != nullptr);
    QVERIFY(
        bodies[orion->bodyIndex].ephemerisSource
        == skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
    );
    QVERIFY(isNear(orion->equatorial.rightAscensionHours, 5.5833, 1e-4));
    QVERIFY(isNear(orion->equatorial.declinationDeg, 5.0, 1e-4));

    QVERIFY(unknownConstellation != nullptr);
    QVERIFY(
        bodies[unknownConstellation->bodyIndex].ephemerisSource
        == skygate::ephemeris::CelestialBodyEphemerisSource::Unresolved
    );
    QVERIFY(std::isnan(unknownConstellation->equatorial.rightAscensionHours));
}

void EphemerisEngineFallbackTests::usesInterfaceDefaultBodyLookupCaseInsensitive()
{
    const SnapshotOnlyEngine engine;
    const skygate::core::SkyContext context;

    const auto state = engine.computeBodyState(context, "hip_42");
    QVERIFY(state.has_value());
    QCOMPARE(state->bodyIndex, 0U);
    QCOMPARE(state->equatorial.rightAscensionHours, 2.0);

    QVERIFY(!engine.computeBodyState(context, "missing").has_value());
}

void EphemerisEngineFallbackTests::usesInterfaceDefaultBodyLookupByIndex()
{
    const SnapshotOnlyEngine engine;
    const skygate::core::SkyContext context;

    const auto state = engine.computeBodyState(context, 0U);
    QVERIFY(state.has_value());
    QCOMPARE(state->bodyIndex, 0U);
    QCOMPARE(state->horizontal.altitudeDeg, 4.0);

    QVERIFY(!engine.computeBodyState(context, 42U).has_value());
}

void EphemerisEngineFallbackTests::skipsHorizontalCoordinatesForInvalidObserver()
{
    const auto catalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("sun", "Sun", skygate::ephemeris::CelestialBodyType::Sun, 0.0),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
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

void EphemerisEngineFallbackTests::fixedCoordinatesOverrideExplicitSourceDispatch()
{
    skygate::ephemeris::CelestialBody body = makeBody(
        "mars",
        "Fixed Mars",
        skygate::ephemeris::CelestialBodyType::Planet,
        0.0,
        skygate::core::EquatorialCoordinate {
            .rightAscensionHours = 3.25,
            .declinationDeg = -12.5
        }
    );
    body.ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Planet;

    const auto catalog = skygate::ephemeris::createStarCatalogFromBodies({body});
    QVERIFY(catalog != nullptr);
    QCOMPARE(
        catalog->bodies()[0].ephemerisSource,
        skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
    );

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 10.0;
    context.observer.longitudeDeg = 20.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1710000000));

    const auto state = engine->computeBodyState(context, "mars");
    QVERIFY(state.has_value());
    QVERIFY(skygate::ephemeris::tests::isNear(
        state->equatorial.rightAscensionHours,
        3.25,
        1e-9
    ));
    QVERIFY(skygate::ephemeris::tests::isNear(
        state->equatorial.declinationDeg,
        -12.5,
        1e-9
    ));
}

QTEST_APPLESS_MAIN(EphemerisEngineFallbackTests)

#include "EphemerisEngineFallbackTests.moc"
