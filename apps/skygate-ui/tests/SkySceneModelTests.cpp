#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySceneModel.hpp"

#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"
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

bool overlayItemsContainText(const QVariantList& overlayItems, const QString& text)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("text").toString() == text) {
            return true;
        }
    }

    return false;
}

}  // namespace

class SkySceneModelTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
    void showsSearchSelectionMarkerForFocusedBody();
    void themeChangesUpdateRenderedColors();
    void solarSystemLabelsCanBeHidden();
    void constellationLabelsAndLinesCanBeHiddenIndependently();
    void referenceLayerLabelsFollowVisibility();
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
    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:00:00"));
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

void SkySceneModelTests::themeChangesUpdateRenderedColors()
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
    controller.setViewCenter(
        demoPlanetState->horizontal.altitudeDeg,
        demoPlanetState->horizontal.azimuthDeg
    );

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QColor defaultPointColor;
    for (const auto& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == demoPlanetState->bodyIndex) {
            defaultPointColor = point.color;
            break;
        }
    }
    QVERIFY(defaultPointColor.isValid());

    const QVariantList defaultOverlayItems = sceneModel.overlayItems();
    QVERIFY(!defaultOverlayItems.isEmpty());
    const QColor defaultOverlayColor =
        defaultOverlayItems.first().toMap().value("color").value<QColor>();
    QVERIFY(defaultOverlayColor.isValid());

    controller.setThemeId("night-vision");

    QColor nightPointColor;
    for (const auto& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == demoPlanetState->bodyIndex) {
            nightPointColor = point.color;
            break;
        }
    }
    QVERIFY(nightPointColor.isValid());
    QVERIFY(defaultPointColor != nightPointColor);

    const QVariantList nightOverlayItems = sceneModel.overlayItems();
    QVERIFY(!nightOverlayItems.isEmpty());
    const QColor nightOverlayColor =
        nightOverlayItems.first().toMap().value("color").value<QColor>();
    QVERIFY(nightOverlayColor.isValid());
    QVERIFY(defaultOverlayColor != nightOverlayColor);
}

void SkySceneModelTests::solarSystemLabelsCanBeHidden()
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
    controller.setViewCenter(
        demoPlanetState->horizontal.altitudeDeg,
        demoPlanetState->horizontal.azimuthDeg
    );

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Demo Planet"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setSolarSystemLabels(false);

    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Demo Planet"));
}

void SkySceneModelTests::constellationLabelsAndLinesCanBeHiddenIndependently()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            1.9,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_25336",
            "HIP 25336",
            skygate::ephemeris::CelestialBodyType::Star,
            2.1,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_25930",
            "HIP 25930",
            skygate::ephemeris::CelestialBodyType::Star,
            2.2,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_26311",
            "HIP 26311",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_26727",
            "HIP 26727",
            skygate::ephemeris::CelestialBodyType::Star,
            2.4,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_24436",
            "HIP 24436",
            skygate::ephemeris::CelestialBodyType::Star,
            2.5,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
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
    QVERIFY(controller.focusSearchTarget("constellationLabel", "Orion"));

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(!sceneModel.renderLineSpan().empty());
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Orion"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setConstellationLabels(false);

    QVERIFY(!sceneModel.renderLineSpan().empty());
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Orion"));

    overlayLayers->setConstellationLines(false);

    QVERIFY(sceneModel.renderLineSpan().empty());
}

void SkySceneModelTests::referenceLayerLabelsFollowVisibility()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            2.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
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
    controller.setViewCenter(35.0, 0.0);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);

    const auto centerOn = [&controller](const skygate::core::HorizontalCoordinate& horizontal) {
        if (!std::isfinite(horizontal.altitudeDeg) || !std::isfinite(horizontal.azimuthDeg)) {
            return false;
        }

        controller.setViewCenter(horizontal.altitudeDeg, horizontal.azimuthDeg);
        return true;
    };

    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
        90.0,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setEcliptic(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));
    overlayLayers->setEcliptic(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));

    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
        24,
        96,
        0.0,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setCelestialEquator(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));
    overlayLayers->setCelestialEquator(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));

    const double boundaryDeclinationDeg =
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
            controller.skyContext().observer
        );
    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
        24,
        96,
        boundaryDeclinationDeg,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setCircumpolarBoundary(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));
    overlayLayers->setCircumpolarBoundary(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));
}

QTEST_GUILESS_MAIN(SkySceneModelTests)

#include "SkySceneModelTests.moc"
