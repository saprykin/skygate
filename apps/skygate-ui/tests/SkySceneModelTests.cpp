#include <QtTest>

#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cmath>
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

class SkySceneModelTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
    void showsSearchSelectionMarkerForFocusedBody();
};

void SkySceneModelTests::buildsFrameAndSupportsHitTesting()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    QVERIFY(ephemerisEngine != nullptr);

    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = false;
    initializationOptions.initializeLocation = false;

    SkyContextController controller(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        initializationOptions,
        nullptr
    );
    controller.setLive(false);
    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");
    const auto snapshot = controller.ephemerisEngine()->compute(controller.skyContext());
    const skygate::ephemeris::CelestialBodyState* demoPlanetState = nullptr;
    for (const auto& state : snapshot.states) {
        if (snapshot.bodyAt(state.bodyIndex).id == "demo_planet") {
            demoPlanetState = &state;
            break;
        }
    }
    QVERIFY(demoPlanetState != nullptr);
    QVERIFY(std::isfinite(demoPlanetState->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(demoPlanetState->horizontal.azimuthDeg));
    controller.setViewCenter(
        demoPlanetState->horizontal.altitudeDeg,
        demoPlanetState->horizontal.azimuthDeg
    );

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(sceneModel.preparedProjection().has_value());
    const auto points = sceneModel.renderPointSpan();
    QVERIFY(!points.empty());

    const SkyRenderPoint* demoPlanetPoint = nullptr;
    for (const auto& point : points) {
        if (point.bodyIndex == demoPlanetState->bodyIndex) {
            demoPlanetPoint = &point;
            break;
        }
    }
    QVERIFY(demoPlanetPoint != nullptr);

    const QString label = sceneModel.objectLabelAt(demoPlanetPoint->x, demoPlanetPoint->y);
    QCOMPARE(label, QString("Demo Planet"));
    QVERIFY(!sceneModel.overlayItems().isEmpty());
}

void SkySceneModelTests::reusesSnapshotAcrossViewChanges()
{
    auto starCatalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    QVERIFY(ephemerisEngine != nullptr);

    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = false;
    initializationOptions.initializeLocation = false;

    SkyContextController controller(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        initializationOptions,
        nullptr
    );
    controller.setLive(false);
    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    const std::uint64_t initialSnapshotGeneration = sceneModel.snapshotGeneration();
    QVERIFY(initialSnapshotGeneration > 0U);

    controller.setViewCenter(40.0, 210.0);
    QCOMPARE(sceneModel.snapshotGeneration(), initialSnapshotGeneration);

    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:30:00"));
    QVERIFY(sceneModel.snapshotGeneration() > initialSnapshotGeneration);
}

void SkySceneModelTests::showsSearchSelectionMarkerForFocusedBody()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    QVERIFY(ephemerisEngine != nullptr);

    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = false;
    initializationOptions.initializeLocation = false;

    SkyContextController controller(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        initializationOptions,
        nullptr
    );
    controller.setLive(false);
    controller.setUtcDateLocked(false);
    controller.setUtcTimeLocked(false);
    controller.setUtcDateText("2024-06-01");
    controller.setUtcTimeText("22:00:00");
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(controller.focusSearchTarget("body", "demo_target"));

    const QVariantMap selectionMarker = sceneModel.selectionMarker();
    QCOMPARE(selectionMarker.value("kind").toString(), QString("searchSelection"));
    QVERIFY(selectionMarker.contains("x"));
    QVERIFY(selectionMarker.contains("y"));
}

QTEST_GUILESS_MAIN(SkySceneModelTests)

#include "SkySceneModelTests.moc"
