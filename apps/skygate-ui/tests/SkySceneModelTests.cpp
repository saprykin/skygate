#include <QtTest>

#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

class SkySceneModelTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
};

void SkySceneModelTests::buildsFrameAndSupportsHitTesting()
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

    QVERIFY(sceneModel.preparedProjection().has_value());
    const auto points = sceneModel.renderPointSpan();
    QVERIFY(!points.empty());

    const QString label = sceneModel.objectLabelAt(points.front().x, points.front().y);
    QVERIFY(!label.isEmpty());
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

    const std::uint64_t initialSnapshotGeneration = sceneModel.snapshotGeneration();
    QVERIFY(initialSnapshotGeneration > 0U);

    controller.setViewCenter(40.0, 210.0);
    QCOMPARE(sceneModel.snapshotGeneration(), initialSnapshotGeneration);

    controller.setUtcTimeText("22:30:00");
    QVERIFY(sceneModel.snapshotGeneration() > initialSnapshotGeneration);
}

QTEST_GUILESS_MAIN(SkySceneModelTests)

#include "SkySceneModelTests.moc"
