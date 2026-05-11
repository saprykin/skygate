#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

using skygate::ui::tests::inspectorFieldValue;
using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::overlayItemsContainKind;
using skygate::ui::tests::SkySceneModelTestHarness;

class SkySceneModelSelectionInspectorTests final : public QObject {
    Q_OBJECT

private slots:
    void showsSearchSelectionMarkerForFocusedBody();
    void clickSelectionBuildsInspectorAndClearsOnEmptyClick();
    void searchSelectionBuildsInspectorNearMarker();
    void circumpolarInspectorShowsObservationFallbacks();
    void inspectorFollowsObjectUnlessPinned();
    void trackedBodyMarkerSurvivesClearedSearchSelectionAndRestoresInspector();
};

void SkySceneModelSelectionInspectorTests::showsSearchSelectionMarkerForFocusedBody()
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

void SkySceneModelSelectionInspectorTests::clickSelectionBuildsInspectorAndClearsOnEmptyClick()
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

void SkySceneModelSelectionInspectorTests::searchSelectionBuildsInspectorNearMarker()
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

void SkySceneModelSelectionInspectorTests::circumpolarInspectorShowsObservationFallbacks()
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

void SkySceneModelSelectionInspectorTests::inspectorFollowsObjectUnlessPinned()
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

void SkySceneModelSelectionInspectorTests::trackedBodyMarkerSurvivesClearedSearchSelectionAndRestoresInspector()
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

QTEST_GUILESS_MAIN(SkySceneModelSelectionInspectorTests)

#include "SkySceneModelSelectionInspectorTests.moc"
