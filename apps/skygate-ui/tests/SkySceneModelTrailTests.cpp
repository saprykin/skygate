#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

using skygate::ui::tests::makeBody;
using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::SkySceneModelTestHarness;

class SkySceneModelTrailTests final : public QObject {
    Q_OBJECT

private slots:
    void selectedBodyBuildsTrailAndClearsIt();
    void trackedBodyTrailSurvivesClearedSearchSelection();
    void constellationSelectionDoesNotBuildTrail();
    void unresolvedBodySelectionDoesNotBuildTrail();
};

void SkySceneModelTrailTests::selectedBodyBuildsTrailAndClearsIt()
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

void SkySceneModelTrailTests::trackedBodyTrailSurvivesClearedSearchSelection()
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

void SkySceneModelTrailTests::constellationSelectionDoesNotBuildTrail()
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

void SkySceneModelTrailTests::unresolvedBodySelectionDoesNotBuildTrail()
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

QTEST_GUILESS_MAIN(SkySceneModelTrailTests)

#include "SkySceneModelTrailTests.moc"
