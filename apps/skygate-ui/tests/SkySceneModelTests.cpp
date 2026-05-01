#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkySceneModel.hpp"
#include "SkyTheme.hpp"
#include "catalog/SkyActiveCatalogBuilder.hpp"

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

bool overlayItemsContainText(const QVariantList& overlayItems, const QString& text)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("text").toString() == text) {
            return true;
        }
    }

    return false;
}

QString inspectorFieldValue(const QVariantMap& inspector, const QString& label)
{
    const QVariantList fields = inspector.value("fields").toList();
    for (const QVariant& fieldValue : fields) {
        const QVariantMap field = fieldValue.toMap();
        if (field.value("label").toString() == label) {
            return field.value("value").toString();
        }
    }

    return {};
}

skygate::ephemeris::CelestialBody makeDeepSkyBody(
    std::string id,
    std::string displayName,
    const double visualMagnitude,
    std::vector<std::string> aliases = {}
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = skygate::ephemeris::CelestialBodyType::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = std::move(aliases),
        .majorAxisArcmin = 8.0,
        .minorAxisArcmin = 4.0,
        .positionAngleDeg = 0.0,
    };
    return body;
}

SkyRenderFrame buildDeepSkyRenderFrame(
    std::vector<skygate::ephemeris::CelestialBody> bodies,
    std::vector<skygate::core::HorizontalCoordinate> horizontalCoordinates,
    const double fovDeg
)
{
    skygate::ephemeris::SkySnapshot snapshot;
    auto catalogBodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>(
        std::move(bodies)
    );
    snapshot.catalogBodies = catalogBodies;
    for (std::uint32_t index = 0; index < catalogBodies->size(); ++index) {
        snapshot.states.push_back(skygate::ephemeris::CelestialBodyState {
            .bodyIndex = index,
            .horizontal = horizontalCoordinates.at(index)
        });
    }

    const auto projectionParams = skygate::core::ViewportMath::buildProjectionParams(
        1000.0,
        700.0,
        45.0,
        180.0,
        fovDeg
    );
    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        projectionParams
    );
    Q_ASSERT(projection.has_value());

    const skygate::ui::internal::SkyThemeRepository themeRepository;
    const SkyRenderFrameBuilder frameBuilder;
    return frameBuilder.buildFrame(
        snapshot,
        *projection,
        {},
        {},
        12.0,
        1000.0,
        700.0,
        themeRepository.defaultTheme().render,
        SkyOverlayLayerVisibility {}
    );
}

}  // namespace

class SkySceneModelTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
    void showsSearchSelectionMarkerForFocusedBody();
    void clickSelectionBuildsInspectorAndClearsOnEmptyClick();
    void searchSelectionBuildsInspectorNearMarker();
    void deepSkyInspectorIncludesAliasesSizeAndSource();
    void primaryDeepSkyObjectKeepsSourceWhenMergingDeepSkyCatalog();
    void themeChangesUpdateRenderedColors();
    void solarSystemLabelsCanBeHidden();
    void deepSkyObjectsRenderAndCanBeHidden();
    void denseDeepSkyLabelsAreBudgeted();
    void wideDeepSkyLabelsPreferNamedObjects();
    void deepestDeepSkyZoomShowsSeparatedAnonymousLabels();
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

void SkySceneModelTests::clickSelectionBuildsInspectorAndClearsOnEmptyClick()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
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
    const auto stateIt = std::find_if(
        snapshot.states.begin(),
        snapshot.states.end(),
        [&snapshot](const skygate::ephemeris::CelestialBodyState& state) {
            return snapshot.bodyAt(state.bodyIndex).id == "demo_star";
        }
    );
    QVERIFY(stateIt != snapshot.states.end());
    controller.setViewCenter(stateIt->horizontal.altitudeDeg, stateIt->horizontal.azimuthDeg);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    const SkyRenderPoint* targetPoint = nullptr;
    for (const auto& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == stateIt->bodyIndex) {
            targetPoint = &point;
            break;
        }
    }
    QVERIFY(targetPoint != nullptr);

    QVERIFY(sceneModel.selectObjectAt(targetPoint->x, targetPoint->y));
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Demo Star"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Star"));
    QCOMPARE(inspectorFieldValue(inspector, "Magnitude"), QString("2.3"));
    QVERIFY(!inspectorFieldValue(inspector, "Alt/Az").isEmpty());
    QVERIFY(!inspectorFieldValue(inspector, "RA/Dec").isEmpty());
    QVERIFY(inspectorFieldValue(inspector, "Angular size").isEmpty());
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    QVERIFY(!sceneModel.selectObjectAt(1.0, 1.0));
    QVERIFY(sceneModel.selectedObjectInspector().isEmpty());
}

void SkySceneModelTests::searchSelectionBuildsInspectorNearMarker()
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
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Demo Target"));
    QVERIFY(inspector.value("x").toDouble() > selectionMarker.value("x").toDouble());
    QVERIFY(inspector.value("y").toDouble() > selectionMarker.value("y").toDouble());
}

void SkySceneModelTests::deepSkyInspectorIncludesAliasesSizeAndSource()
{
    skygate::ephemeris::CelestialBody m31 = makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        {"M 31", "NGC 224", "Andromeda Galaxy"}
    );
    m31.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 0.7123,
        .declinationDeg = 41.269
    };

    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({m31});
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
    QVERIFY(controller.setUtcDateTimeText("2024-09-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");

    const auto snapshot = controller.ephemerisEngine()->compute(controller.skyContext());
    const auto stateIt = std::find_if(
        snapshot.states.begin(),
        snapshot.states.end(),
        [&snapshot](const skygate::ephemeris::CelestialBodyState& state) {
            return snapshot.bodyAt(state.bodyIndex).displayName == "M31";
        }
    );
    QVERIFY(stateIt != snapshot.states.end());
    controller.setViewCenter(stateIt->horizontal.altitudeDeg, stateIt->horizontal.azimuthDeg);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(!sceneModel.renderGlyphSpan().empty());
    const auto glyphIt = std::find_if(
        sceneModel.renderGlyphSpan().begin(),
        sceneModel.renderGlyphSpan().end(),
        [&sceneModel](const SkyRenderGlyph& glyph) {
            return sceneModel.objectLabelAt(glyph.x, glyph.y) == "M31";
        }
    );
    QVERIFY(glyphIt != sceneModel.renderGlyphSpan().end());

    QVERIFY(sceneModel.selectObjectAt(glyphIt->x, glyphIt->y));
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("M31"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Galaxy"));
    QVERIFY(inspectorFieldValue(inspector, "Angular size").contains("arcmin"));
    QCOMPARE(inspectorFieldValue(inspector, "Source"), QString("Bundled Messier"));
    QVERIFY(inspector.value("aliases").toString().contains("Andromeda Galaxy"));
}

void SkySceneModelTests::primaryDeepSkyObjectKeepsSourceWhenMergingDeepSkyCatalog()
{
    auto primaryCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyBody(
            "primary_dso",
            "Primary DSO",
            8.0,
            {"Primary DSO"}
        ),
        makeDeepSkyBody(
            "messier_031",
            "M31",
            3.44,
            {"M 31", "NGC 224"}
        ),
    });
    QVERIFY(primaryCatalog != nullptr);

    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyBody(
            "open_ngc_m31",
            "OpenNGC M31",
            3.4,
            {"M 31", "NGC 224", "Andromeda Galaxy"}
        ),
    });
    QVERIFY(deepSkyCatalog != nullptr);

    const auto buildResult = skygate::ui::internal::SkyActiveCatalogBuilder::build(
        skygate::ui::internal::SkyActiveCatalogBuildRequest {
            .sourceCatalog = *primaryCatalog,
            .deepSkyCatalog = deepSkyCatalog.get(),
            .sourceLabel = "Primary",
            .deepSkySourceLabel = "OpenNGC"
        }
    );
    QVERIFY(buildResult.isSuccess());
    QCOMPARE(buildResult.sourceIds.size(), buildResult.catalog->bodies().size());

    const int primarySourceIndex = buildResult.sourceLabels.indexOf("Primary");
    const int deepSkySourceIndex = buildResult.sourceLabels.indexOf("OpenNGC");
    QVERIFY(primarySourceIndex >= 0);
    QVERIFY(deepSkySourceIndex >= 0);

    bool sawPrimaryDso = false;
    bool sawOpenNgcM31 = false;
    for (std::size_t index = 0; index < buildResult.catalog->bodies().size(); ++index) {
        const auto& body = buildResult.catalog->bodies()[index];
        if (body.id == "primary_dso") {
            sawPrimaryDso = true;
            QCOMPARE(buildResult.sourceIds[index], static_cast<std::uint8_t>(primarySourceIndex));
        } else if (body.id == "open_ngc_m31") {
            sawOpenNgcM31 = true;
            QCOMPARE(buildResult.sourceIds[index], static_cast<std::uint8_t>(deepSkySourceIndex));
        } else if (body.id == "messier_031") {
            QFAIL("Merged catalog should replace the primary M31 with the OpenNGC body");
        }
    }

    QVERIFY(sawPrimaryDso);
    QVERIFY(sawOpenNgcM31);
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

void SkySceneModelTests::deepSkyObjectsRenderAndCanBeHidden()
{
    skygate::ephemeris::CelestialBody m31 = makeBody(
        "messier_031",
        "M31",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        3.44,
        skygate::core::EquatorialCoordinate {
            .rightAscensionHours = 0.7123,
            .declinationDeg = 41.269
        }
    );
    m31.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"M31", "Andromeda Galaxy"},
        .majorAxisArcmin = 177.0,
        .minorAxisArcmin = 70.0,
        .positionAngleDeg = 35.0,
    };

    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({m31});
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
    QVERIFY(controller.setUtcDateTimeText("2024-09-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");

    const auto snapshot = controller.ephemerisEngine()->compute(controller.skyContext());
    const auto m31StateIt = std::find_if(
        snapshot.states.begin(),
        snapshot.states.end(),
        [&snapshot](const skygate::ephemeris::CelestialBodyState& state) {
            return snapshot.bodyAt(state.bodyIndex).displayName == "M31";
        }
    );
    QVERIFY(m31StateIt != snapshot.states.end());
    const auto& state = *m31StateIt;
    QVERIFY(std::isfinite(state.horizontal.altitudeDeg));
    QVERIFY(std::isfinite(state.horizontal.azimuthDeg));
    controller.setViewCenter(state.horizontal.altitudeDeg, state.horizontal.azimuthDeg);

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(!sceneModel.renderGlyphSpan().empty());
    QVERIFY(std::any_of(
        sceneModel.renderGlyphSpan().begin(),
        sceneModel.renderGlyphSpan().end(),
        [&sceneModel](const SkyRenderGlyph& glyph) {
            return sceneModel.objectLabelAt(glyph.x, glyph.y) == "M31";
        }
    ));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setDeepSkyObjects(false);

    QVERIFY(sceneModel.renderGlyphSpan().empty());
}

void SkySceneModelTests::denseDeepSkyLabelsAreBudgeted()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    std::vector<skygate::core::HorizontalCoordinate> coordinates;
    bodies.reserve(360U);
    coordinates.reserve(360U);

    for (int index = 0; index < 360; ++index) {
        const int row = index / 24;
        const int column = index % 24;
        bodies.push_back(makeDeepSkyBody(
            "ngc_" + std::to_string(index + 1),
            "NGC " + std::to_string(index + 1),
            9.0 + (static_cast<double>(index % 5) * 0.1)
        ));
        coordinates.push_back(skygate::core::HorizontalCoordinate {
            .altitudeDeg = 45.0 + ((static_cast<double>(row) - 7.0) * 0.16),
            .azimuthDeg = 180.0 + ((static_cast<double>(column) - 12.0) * 0.16)
        });
    }

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        std::move(coordinates),
        10.0
    );

    QCOMPARE(frame.glyphs.size(), 360U);
    QVERIFY(frame.labels.size() > 0);
    QVERIFY(frame.labels.size() <= 100);
}

void SkySceneModelTests::wideDeepSkyLabelsPreferNamedObjects()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.push_back(makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        {"M 31", "NGC 224", "Andromeda Galaxy"}
    ));
    bodies.push_back(makeDeepSkyBody(
        "ngc_100",
        "NGC 100",
        4.0,
        {"NGC 100"}
    ));
    bodies.push_back(makeDeepSkyBody(
        "ngc_7000",
        "NGC 7000",
        4.0,
        {"NGC 7000", "North America Nebula"}
    ));

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        {
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 180.0
            },
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 195.0
            },
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 165.0
            }
        },
        50.0
    );

    QVERIFY(overlayItemsContainText(frame.labels, "M31"));
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7000"));
    QVERIFY(!overlayItemsContainText(frame.labels, "NGC 100"));
}

void SkySceneModelTests::deepestDeepSkyZoomShowsSeparatedAnonymousLabels()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    std::vector<skygate::core::HorizontalCoordinate> coordinates;
    bodies.reserve(24U);
    coordinates.reserve(24U);

    for (int index = 0; index < 24; ++index) {
        const int row = index / 6;
        const int column = index % 6;
        bodies.push_back(makeDeepSkyBody(
            "ngc_" + std::to_string(7000 + index),
            "NGC " + std::to_string(7000 + index),
            10.0
        ));
        coordinates.push_back(skygate::core::HorizontalCoordinate {
            .altitudeDeg = 45.0 + ((static_cast<double>(row) - 1.5) * 0.16),
            .azimuthDeg = 180.0 + ((static_cast<double>(column) - 2.5) * 0.16)
        });
    }

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        std::move(coordinates),
        skygate::core::ViewportMath::kFieldOfViewMinDeg
    );

    QCOMPARE(frame.glyphs.size(), 24U);
    QCOMPARE(frame.labels.size(), 24);
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7000"));
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7023"));
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
