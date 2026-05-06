#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkySceneModel.hpp"
#include "SkySceneModelTestHarness.hpp"
#include "SkyTimeController.hpp"
#include "SkyTheme.hpp"
#include "catalog/SkyActiveCatalogBuilder.hpp"

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using skygate::ui::tests::createTestCatalog;
using skygate::ui::tests::makeBody;
using skygate::ui::tests::makeDeepSkyBody;
using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::SkySceneModelTestHarness;
using skygate::ui::tests::TestSkyContextConfig;

bool overlayItemsContainText(const QVariantList& overlayItems, const QString& text)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("text").toString() == text) {
            return true;
        }
    }

    return false;
}

bool overlayItemsContainText(const std::vector<SkyRenderLabel>& overlayItems, const QString& text)
{
    for (const SkyRenderLabel& overlayItem : overlayItems) {
        if (overlayItem.text == text) {
            return true;
        }
    }

    return false;
}

bool overlayItemsContainKind(const QVariantList& overlayItems, const QString& kind)
{
    for (const QVariant& overlayItem : overlayItems) {
        if (overlayItem.toMap().value("kind").toString() == kind) {
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
    void circumpolarInspectorShowsObservationFallbacks();
    void inspectorFollowsObjectUnlessPinned();
    void trackedBodyMarkerSurvivesClearedSearchSelectionAndRestoresInspector();
    void selectedBodyBuildsTrailAndClearsIt();
    void trackedBodyTrailSurvivesClearedSearchSelection();
    void constellationSelectionDoesNotBuildTrail();
    void unresolvedBodySelectionDoesNotBuildTrail();
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
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));

    const SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(sceneModel.preparedProjection().has_value());
    const auto points = sceneModel.renderPointSpan();
    QVERIFY(!points.empty());

    const SkyRenderPoint* demoPlanetPoint = harness.renderPointForBodyId("demo_planet");
    QVERIFY(demoPlanetPoint != nullptr);

    const QString label = sceneModel.objectLabelAt(demoPlanetPoint->x, demoPlanetPoint->y);
    QCOMPARE(label, QString("Demo Planet"));
    QVERIFY(!sceneModel.overlayItems().isEmpty());
}

void SkySceneModelTests::reusesSnapshotAcrossViewChanges()
{
    SkySceneModelTestHarness harness = SkySceneModelTestHarness::fromBundledCatalog();
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    const std::uint64_t initialSnapshotGeneration = sceneModel.snapshotGeneration();
    QVERIFY(initialSnapshotGeneration > 0U);

    controller.setViewCenter(40.0, 210.0);
    QCOMPARE(sceneModel.snapshotGeneration(), initialSnapshotGeneration);

    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:30:00"));
    QVERIFY(sceneModel.snapshotGeneration() > initialSnapshotGeneration);
}

void SkySceneModelTests::showsSearchSelectionMarkerForFocusedBody()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(controller.focusSearchTarget("body", "demo_target"));

    const QVariantMap selectionMarker = sceneModel.selectionMarker();
    QCOMPARE(selectionMarker.value("kind").toString(), QString("searchSelection"));
    QVERIFY(selectionMarker.contains("x"));
    QVERIFY(selectionMarker.contains("y"));
}

void SkySceneModelTests::clickSelectionBuildsInspectorAndClearsOnEmptyClick()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_star"));
    SkySceneModel& sceneModel = harness.sceneModel();

    const SkyRenderPoint* targetPoint = harness.renderPointForBodyId("demo_star");
    QVERIFY(targetPoint != nullptr);

    QVERIFY(sceneModel.selectObjectAt(targetPoint->x, targetPoint->y));
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Demo Star"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Star"));
    QCOMPARE(inspectorFieldValue(inspector, "Magnitude"), QString("2.3"));
    QVERIFY(!inspectorFieldValue(inspector, "Alt / Az").isEmpty());
    QVERIFY(!inspectorFieldValue(inspector, "RA / Dec").isEmpty());
    QVERIFY(inspectorFieldValue(inspector, "Rise").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Set").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("deg"));
    QVERIFY(inspectorFieldValue(inspector, "Angular size").isEmpty());
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    QVERIFY(!sceneModel.selectObjectAt(1.0, 1.0));
    QVERIFY(sceneModel.selectedObjectInspector().isEmpty());
}

void SkySceneModelTests::searchSelectionBuildsInspectorNearMarker()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(controller.focusSearchTarget("body", "demo_target"));

    const QVariantMap selectionMarker = sceneModel.selectionMarker();
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Demo Target"));
    QVERIFY(inspectorFieldValue(inspector, "Rise").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Set").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("deg"));
    QVERIFY(sceneModel.renderLineSpan().size() > 0U);
    QVERIFY(inspector.value("x").toDouble() > selectionMarker.value("x").toDouble());
    QVERIFY(inspector.value("y").toDouble() > selectionMarker.value("y").toDouble());
}

void SkySceneModelTests::circumpolarInspectorShowsObservationFallbacks()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_circumpolar",
            "Demo Circumpolar",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            3.0,
            80.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(controller.focusSearchTarget("body", "demo_circumpolar"));

    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Demo Circumpolar"));
    QCOMPARE(inspectorFieldValue(inspector, "Rise"), QString("Always above"));
    QCOMPARE(inspectorFieldValue(inspector, "Set"), QString("Always above"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("UTC"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("deg"));
}

void SkySceneModelTests::inspectorFollowsObjectUnlessPinned()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(controller.focusSearchTarget("body", "demo_target"));

    const QVariantMap initialInspector = sceneModel.selectedObjectInspector();
    QVERIFY(initialInspector.value("visible").toBool());
    QVERIFY(!initialInspector.value("pinned").toBool());
    const double initialX = initialInspector.value("x").toDouble();
    const double initialY = initialInspector.value("y").toDouble();
    const std::uint64_t snapshotBeforeTimeZoneChange = sceneModel.snapshotGeneration();
    QVERIFY(inspectorFieldValue(initialInspector, "Rise").contains("UTC"));
    QVERIFY(inspectorFieldValue(initialInspector, "Set").contains("UTC"));
    QVERIFY(controller.timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QCOMPARE(sceneModel.snapshotGeneration(), snapshotBeforeTimeZoneChange);
    QVERIFY(
        inspectorFieldValue(
            sceneModel.selectedObjectInspector(),
            "Rise"
        ).contains("UTC+06:00")
    );
    QVERIFY(controller.timeController()->setTimeZoneId(QStringLiteral("UTC")));
    QCOMPARE(sceneModel.snapshotGeneration(), snapshotBeforeTimeZoneChange);

    controller.panViewBy(2.0, 0.0);
    const QVariantMap followedInspector = sceneModel.selectedObjectInspector();
    QVERIFY(followedInspector.value("visible").toBool());
    QVERIFY(
        std::abs(followedInspector.value("x").toDouble() - initialX) > 1e-3
        || std::abs(followedInspector.value("y").toDouble() - initialY) > 1e-3
    );

    sceneModel.moveSelectedObjectInspector(120.0, 140.0);
    QVariantMap pinnedInspector = sceneModel.selectedObjectInspector();
    QVERIFY(pinnedInspector.value("pinned").toBool());
    QCOMPARE(pinnedInspector.value("x").toDouble(), 120.0);
    QCOMPARE(pinnedInspector.value("y").toDouble(), 140.0);
    const QString pinnedRiseText = inspectorFieldValue(pinnedInspector, "Rise");

    controller.panViewBy(2.0, 0.0);
    pinnedInspector = sceneModel.selectedObjectInspector();
    QVERIFY(pinnedInspector.value("pinned").toBool());
    QCOMPARE(pinnedInspector.value("x").toDouble(), 120.0);
    QCOMPARE(pinnedInspector.value("y").toDouble(), 140.0);

    QVERIFY(controller.setUtcDateTimeText("2024-06-02", "22:00:00"));
    pinnedInspector = sceneModel.selectedObjectInspector();
    QVERIFY(pinnedInspector.value("pinned").toBool());
    QCOMPARE(pinnedInspector.value("x").toDouble(), 120.0);
    QCOMPARE(pinnedInspector.value("y").toDouble(), 140.0);
    QVERIFY(inspectorFieldValue(pinnedInspector, "Rise").contains("UTC"));
    QVERIFY(inspectorFieldValue(pinnedInspector, "Rise") != pinnedRiseText);

    sceneModel.setSelectedObjectInspectorPinned(false);
    const QVariantMap unpinnedInspector = sceneModel.selectedObjectInspector();
    QVERIFY(!unpinnedInspector.value("pinned").toBool());
    QVERIFY(
        std::abs(unpinnedInspector.value("x").toDouble() - 120.0) > 1e-3
        || std::abs(unpinnedInspector.value("y").toDouble() - 140.0) > 1e-3
    );
}

void SkySceneModelTests::trackedBodyMarkerSurvivesClearedSearchSelectionAndRestoresInspector()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(controller.trackSearchTarget("body", "demo_target"));
    QCOMPARE(sceneModel.selectedObjectInspector().value("title").toString(), QString("Demo Target"));
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    controller.clearSelectedSearchTarget();
    QVERIFY(sceneModel.selectedObjectInspector().isEmpty());
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    QVERIFY(controller.focusSearchTarget(controller.trackedTargetKind(), controller.trackedTargetId()));
    QCOMPARE(sceneModel.selectedObjectInspector().value("title").toString(), QString("Demo Target"));

    controller.clearTrackedTarget();
    QVERIFY(!controller.hasTrackedTarget());
    QVERIFY(!sceneModel.selectionMarker().isEmpty());
}

void SkySceneModelTests::selectedBodyBuildsTrailAndClearsIt()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_trail",
            "Demo Trail",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            5.5,
            20.0
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_trail"));
    SkySceneModel& sceneModel = harness.sceneModel();

    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));

    const SkyRenderPoint* targetPoint = harness.renderPointForBodyId("demo_trail");
    QVERIFY(targetPoint != nullptr);

    QVERIFY(sceneModel.selectObjectAt(targetPoint->x, targetPoint->y));
    QVERIFY(sceneModel.renderLineSpan().size() > 0U);

    QVERIFY(!sceneModel.selectObjectAt(1.0, 1.0));
    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));
}

void SkySceneModelTests::trackedBodyTrailSurvivesClearedSearchSelection()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_tracked_trail",
            "Demo Tracked Trail",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            5.5,
            20.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(controller.trackSearchTarget("body", "demo_tracked_trail"));
    QVERIFY(sceneModel.renderLineSpan().size() > 0U);

    controller.clearSelectedSearchTarget();
    QVERIFY(sceneModel.renderLineSpan().size() > 0U);

    controller.clearTrackedTarget();
    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));
}

void SkySceneModelTests::constellationSelectionDoesNotBuildTrail()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            1.9,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25336",
            "HIP 25336",
            skygate::ephemeris::CelestialBodyType::Star,
            2.1,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25930",
            "HIP 25930",
            skygate::ephemeris::CelestialBodyType::Star,
            2.2,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26311",
            "HIP 26311",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26727",
            "HIP 26727",
            skygate::ephemeris::CelestialBodyType::Star,
            2.4,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_24436",
            "HIP 24436",
            skygate::ephemeris::CelestialBodyType::Star,
            2.5,
            5.5,
            5.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setConstellationLines(false);

    QVERIFY(controller.focusSearchTarget("constellationLabel", "Orion"));
    QVERIFY(!sceneModel.selectionMarker().isEmpty());
    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));
}

void SkySceneModelTests::unresolvedBodySelectionDoesNotBuildTrail()
{
    SkySceneModelTestHarness harness({
        makeBody(
            "unresolved_target",
            "Unresolved Target",
            skygate::ephemeris::CelestialBodyType::DeepSkyObject,
            8.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setConstellationLines(false);

    QVERIFY(!controller.focusSearchTarget("body", "unresolved_target"));
    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));
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

    TestSkyContextConfig contextConfig;
    contextConfig.utcDate = QStringLiteral("2024-09-01");
    SkySceneModelTestHarness harness({std::move(m31)}, contextConfig);
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("messier_031"));
    SkyContextController& controller = harness.controller();
    SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(!sceneModel.renderGlyphSpan().empty());
    const std::optional<SkyRenderGlyph> glyph = harness.renderGlyphWithLabel(QStringLiteral("M31"));
    QVERIFY(glyph.has_value());

    QVERIFY(sceneModel.selectObjectAt(glyph->x, glyph->y));
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("M31"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Galaxy"));
    QVERIFY(inspectorFieldValue(inspector, "Angular size").contains("arcmin"));
    QCOMPARE(inspectorFieldValue(inspector, "Source"), QString("Bundled Messier"));
    QVERIFY(inspector.value("aliases").toString().contains("Andromeda Galaxy"));

    QVERIFY(controller.focusSearchTarget("body", "messier_031"));
    QCOMPARE(sceneModel.selectedObjectInspector().value("title").toString(), QString("M31"));
    QVERIFY(!overlayItemsContainKind(sceneModel.overlayItems(), "selectionLabel"));
}

void SkySceneModelTests::primaryDeepSkyObjectKeepsSourceWhenMergingDeepSkyCatalog()
{
    auto primaryCatalog = createTestCatalog({
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

    auto deepSkyCatalog = createTestCatalog({
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
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    const auto demoPlanetState = harness.bodyStateById("demo_planet");
    QVERIFY(demoPlanetState.has_value());

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
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

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

    TestSkyContextConfig contextConfig;
    contextConfig.utcDate = QStringLiteral("2024-09-01");
    SkySceneModelTestHarness harness({std::move(m31)}, contextConfig);
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("messier_031"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

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
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            1.9,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25336",
            "HIP 25336",
            skygate::ephemeris::CelestialBodyType::Star,
            2.1,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25930",
            "HIP 25930",
            skygate::ephemeris::CelestialBodyType::Star,
            2.2,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26311",
            "HIP 26311",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26727",
            "HIP 26727",
            skygate::ephemeris::CelestialBodyType::Star,
            2.4,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_24436",
            "HIP 24436",
            skygate::ephemeris::CelestialBodyType::Star,
            2.5,
            5.5,
            5.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(controller.focusSearchTarget("constellationLabel", "Orion"));

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
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            2.0,
            5.5,
            5.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    controller.setViewCenter(35.0, 0.0);

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
