#include "support/ConstellationTestSupport.hpp"
#include "support/SkySceneModelTestSupport.hpp"

#include <QtTest>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::restoreSeededCatalogCache;
using skygate::ui::tests::seedOrionConstellationCache;
using skygate::ui::tests::SettingsTestFixture;
using skygate::ui::tests::SkySceneModelTestHarness;

class SkySceneModelViewportSnapshotTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void publishesProjectionContextAndPrimitives();
    void inspectorEditsKeepViewportSnapshot();
    void viewAndTimeChangesBumpViewportSnapshot();
    void themeAndVisibilityChangesBumpViewport();
    void resizeBumpsViewportSnapshot();
    void trailSelectionBumpsViewportSnapshot();
    void catalogChangeBumpsViewportSnapshot();

private:
    SettingsTestFixture m_settings;
};

void SkySceneModelViewportSnapshotTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkySceneModelViewportSnapshotTests")));
}

void SkySceneModelViewportSnapshotTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkySceneModelViewportSnapshotTests::publishesProjectionContextAndPrimitives()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet", "Demo Planet", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -1.0, 1.5, 2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));
    const SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    const std::shared_ptr<const SkyViewportSnapshot> snapshot = sceneModel.viewportSnapshot();
    QVERIFY(snapshot != nullptr);
    QVERIFY(snapshot->generation > 0U);
    QVERIFY(snapshot->preparedProjection.has_value());
    QCOMPARE(snapshot->viewportWidth, 1100.0);
    QCOMPARE(snapshot->viewportHeight, 760.0);
    QCOMPARE(snapshot->preparedProjection->params().viewportWidth, 1100.0);
    QCOMPARE(snapshot->preparedProjection->params().viewportHeight, 760.0);

    const std::optional<skygate::core::ObservationContext> referenceContext = sceneModel.referenceOverlayContext();
    QVERIFY(referenceContext.has_value());
    QCOMPARE(snapshot->observationContext.observer.latitudeDeg, referenceContext->observer.latitudeDeg);
    QCOMPARE(snapshot->observationContext.observer.longitudeDeg, referenceContext->observer.longitudeDeg);
    QVERIFY(snapshot->observationContext.utcTime == referenceContext->utcTime);

    QCOMPARE(snapshot->renderTheme.gridAzimuthLine, controller.renderTheme().gridAzimuthLine);
    QVERIFY(snapshot->overlayLayers.equals(controller.overlayLayerVisibility()));

    QCOMPARE(snapshot->points.size(), sceneModel.renderPointSpan().size());
    QCOMPARE(snapshot->lines.size(), sceneModel.renderLineSpan().size());
    QCOMPARE(snapshot->glyphs.size(), sceneModel.renderGlyphSpan().size());
    QVERIFY(!snapshot->points.empty());
}

void SkySceneModelViewportSnapshotTests::inspectorEditsKeepViewportSnapshot()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_star", "Demo Star", skygate::ephemeris::BaseCelestialBody::Kind::Star, 2.3, 1.5, 2.5),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_star"));
    SkyContextController& controller = harness.controller();
    SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(controller.focusSearchTarget("body", "demo_star"));

    const std::shared_ptr<const SkyViewportSnapshot> baseline = sceneModel.viewportSnapshot();
    QVERIFY(baseline != nullptr);
    QVERIFY(baseline->preparedProjection.has_value());
    const std::uint64_t baselineGeneration = baseline->generation;

    sceneModel.moveSelectedObjectInspector(120.0, 140.0);
    QCOMPARE(sceneModel.viewportSnapshot().get(), baseline.get());
    QCOMPARE(sceneModel.viewportSnapshot()->generation, baselineGeneration);

    sceneModel.setSelectedObjectInspectorPinned(false);
    QCOMPARE(sceneModel.viewportSnapshot().get(), baseline.get());
    QCOMPARE(sceneModel.viewportSnapshot()->generation, baselineGeneration);

    QVERIFY(controller.timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QCOMPARE(sceneModel.viewportSnapshot().get(), baseline.get());
    QCOMPARE(sceneModel.viewportSnapshot()->generation, baselineGeneration);
}

void SkySceneModelViewportSnapshotTests::viewAndTimeChangesBumpViewportSnapshot()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_star", "Demo Star", skygate::ephemeris::BaseCelestialBody::Kind::Star, 2.3, 1.5, 2.5),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_star"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    const std::uint64_t baselineGeneration = sceneModel.viewportSnapshot()->generation;
    controller.panViewBy(2.0, 0.0);
    const std::uint64_t viewGeneration = sceneModel.viewportSnapshot()->generation;
    QVERIFY(viewGeneration > baselineGeneration);

    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("2024-06-02"), QStringLiteral("22:00:00")));
    QVERIFY(sceneModel.viewportSnapshot()->generation > viewGeneration);
}

void SkySceneModelViewportSnapshotTests::themeAndVisibilityChangesBumpViewport()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_star", "Demo Star", skygate::ephemeris::BaseCelestialBody::Kind::Star, 2.3, 1.5, 2.5),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_star"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    const std::uint64_t baselineGeneration = sceneModel.viewportSnapshot()->generation;
    controller.setThemeId(QStringLiteral("night-vision"));
    const std::uint64_t themeGeneration = sceneModel.viewportSnapshot()->generation;
    QVERIFY(themeGeneration > baselineGeneration);
    QCOMPARE(sceneModel.viewportSnapshot()->renderTheme.gridAzimuthLine, controller.renderTheme().gridAzimuthLine);

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setAltAzGrid(false);
    QVERIFY(sceneModel.viewportSnapshot()->generation > themeGeneration);
    QVERIFY(!sceneModel.viewportSnapshot()->overlayLayers.altAzGrid);
}

void SkySceneModelViewportSnapshotTests::resizeBumpsViewportSnapshot()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_star", "Demo Star", skygate::ephemeris::BaseCelestialBody::Kind::Star, 2.3, 1.5, 2.5),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_star"));
    SkySceneModel& sceneModel = harness.sceneModel();

    const std::shared_ptr<const SkyViewportSnapshot> baseline = sceneModel.viewportSnapshot();
    QVERIFY(baseline != nullptr);

    sceneModel.setViewportSize(1200.0, 800.0);
    const std::shared_ptr<const SkyViewportSnapshot> resized = sceneModel.viewportSnapshot();
    QVERIFY(resized->generation > baseline->generation);
    QCOMPARE(resized->viewportWidth, 1200.0);
    QCOMPARE(resized->viewportHeight, 800.0);
    QCOMPARE(resized->preparedProjection->params().viewportWidth, 1200.0);
    QCOMPARE(resized->preparedProjection->params().viewportHeight, 800.0);

    sceneModel.setViewportSize(1200.0, 800.0);
    QCOMPARE(sceneModel.viewportSnapshot().get(), resized.get());
}

void SkySceneModelViewportSnapshotTests::trailSelectionBumpsViewportSnapshot()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_trail", "Demo Trail", skygate::ephemeris::BaseCelestialBody::Kind::Star, 1.0, 5.5, 20.0),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_trail"));
    SkySceneModel& sceneModel = harness.sceneModel();

    const SkyRenderPoint* targetPoint = harness.renderPointForBodyId("demo_trail");
    QVERIFY(targetPoint != nullptr);
    QCOMPARE(sceneModel.renderLineSpan().size(), std::size_t(0));

    const std::uint64_t baselineGeneration = sceneModel.viewportSnapshot()->generation;
    QVERIFY(sceneModel.selectObjectAt(targetPoint->x, targetPoint->y));
    const std::shared_ptr<const SkyViewportSnapshot> selected = sceneModel.viewportSnapshot();
    QVERIFY(selected->generation > baselineGeneration);
    QVERIFY(!selected->lines.empty());
}

void SkySceneModelViewportSnapshotTests::catalogChangeBumpsViewportSnapshot()
{
    QVERIFY(seedOrionConstellationCache());
    SkySceneModelTestHarness harness({
        makeFixedBody("placeholder", "Placeholder", skygate::ephemeris::BaseCelestialBody::Kind::Star, 6.0, 1.0, 1.0),
    });
    QVERIFY(harness.isValid());
    const SkySceneModel& sceneModel = harness.sceneModel();
    const std::uint64_t baselineGeneration = sceneModel.viewportSnapshot()->generation;

    restoreSeededCatalogCache(harness.controller());
    QTRY_VERIFY(sceneModel.viewportSnapshot()->generation > baselineGeneration);
}

QTEST_GUILESS_MAIN(SkySceneModelViewportSnapshotTests)

#include "SkySceneModelViewportSnapshotTests.moc"
