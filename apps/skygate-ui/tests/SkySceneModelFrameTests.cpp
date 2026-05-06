#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::SkySceneModelTestHarness;

class SkySceneModelFrameTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
};

void SkySceneModelFrameTests::buildsFrameAndSupportsHitTesting()
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

void SkySceneModelFrameTests::reusesSnapshotAcrossViewChanges()
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

QTEST_GUILESS_MAIN(SkySceneModelFrameTests)

#include "SkySceneModelFrameTests.moc"
