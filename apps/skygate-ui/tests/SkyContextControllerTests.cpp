#include <QtTest>

#include "SkyContextController.hpp"
#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <limits>
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

std::unique_ptr<skygate::ephemeris::IStarCatalog> createTestCatalog()
{
    return skygate::ephemeris::createBundledStarCatalog();
}

std::unique_ptr<skygate::ephemeris::IEphemerisEngine> createTestEphemerisEngine(
    const skygate::ephemeris::IStarCatalog& starCatalog
)
{
    return skygate::ephemeris::createEphemerisEngine(starCatalog);
}

SkyContextController::InitializationOptions controllerInitializationOptions(bool loadSettings)
{
    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = loadSettings;
    initializationOptions.initializeLocation = false;
    return initializationOptions;
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    bool loadSettings = false
);

std::unique_ptr<SkyContextController> createController(bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings);
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings
)
{
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        controllerInitializationOptions(loadSettings),
        nullptr
    );
}

QDateTime controllerUtcTime(const SkyContextController& controller)
{
    return skygate::ui::internal::SkyContextTimeCodec::toQDateTimeUtc(controller.skyContext().utcTime);
}

const skygate::ephemeris::CelestialBodyState* findStateById(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const std::string& bodyId
)
{
    for (const auto& state : snapshot.states) {
        if (snapshot.bodyAt(state.bodyIndex).id == bodyId) {
            return &state;
        }
    }

    return nullptr;
}

void configureFocusTestContext(SkyContextController& controller)
{
    controller.setLive(false);
    controller.setUtcDateLocked(false);
    controller.setUtcTimeLocked(false);
    controller.setUtcDateText("2024-06-01");
    controller.setUtcTimeText("22:00:00");
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");
}

double azimuthDifferenceDeg(const double lhs, const double rhs)
{
    const double difference = std::abs(lhs - rhs);
    return std::min(difference, 360.0 - difference);
}

}  // namespace

class SkyContextControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void pinchZoomScaleDeltaAdjustsAndClampsFieldOfView();
    void pinchZoomScaleDeltaIgnoresInvalidValues();
    void selectingCityUpdatesCoordinatesAndPreservesElevation();
    void manualCoordinateEditSwitchesToCustom();
    void invalidSavedCityFallsBackToCustom();
    void defaultsLocationSourceByPositioningAvailability();
    void enablingUtcLocksDoesNotSnapTimelineToCurrentUtc();
    void lockedUtcDateBlocksManualDateEdits();
    void lockedUtcTimeBlocksManualTimeEdits();
    void manualTimelineSteppingStillWorksWithUtcEditLocksEnabled();
    void livePlaybackUsesManualStepWhileCatchingUpWithUtcLocksEnabled();
    void livePlaybackUsesManualStepWhileCatchingUp();
    void livePlaybackDoesNotOvershootCurrentUtcWhenCatchingUp();
    void livePlaybackFallsBackToOneSecondTicksAfterCatchUp();
    void goLiveNowJumpsToCurrentUtcAndEnablesLive();
    void focusSearchTargetCentersBodyResult();
    void focusSearchTargetCentersConstellationLabelResult();
    void focusSearchTargetIgnoresInvalidTargets();
    void collapsingSearchToolbarClearsSelectedSearchTarget();

private:
    QTemporaryDir m_settingsDir;
};

void SkyContextControllerTests::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QCoreApplication::setOrganizationName("SkygateTests");
    QCoreApplication::setApplicationName("SkygateUiControllerTests");
}

void SkyContextControllerTests::init()
{
    QSettings settings;
    settings.clear();
}

void SkyContextControllerTests::pinchZoomScaleDeltaAdjustsAndClampsFieldOfView()
{
    const auto controller = createController();

    controller->zoomViewByScaleDelta(1.25);
    QCOMPARE(controller->viewFieldOfViewDeg(), 80.0);

    controller->zoomViewByScaleDelta(0.5);
    QCOMPARE(
        controller->viewFieldOfViewDeg(),
        skygate::core::ViewportMath::kFieldOfViewMaxDeg
    );

    controller->zoomViewByScaleDelta(10.0);
    QCOMPARE(
        controller->viewFieldOfViewDeg(),
        skygate::core::ViewportMath::kFieldOfViewMinDeg
    );
}

void SkyContextControllerTests::pinchZoomScaleDeltaIgnoresInvalidValues()
{
    const auto controller = createController();
    const double initialFieldOfViewDeg = controller->viewFieldOfViewDeg();

    controller->zoomViewByScaleDelta(1.0);
    controller->zoomViewByScaleDelta(0.0);
    controller->zoomViewByScaleDelta(-0.5);
    controller->zoomViewByScaleDelta(std::numeric_limits<double>::quiet_NaN());
    controller->zoomViewByScaleDelta(std::numeric_limits<double>::infinity());

    QCOMPARE(controller->viewFieldOfViewDeg(), initialFieldOfViewDeg);
}

void SkyContextControllerTests::selectingCityUpdatesCoordinatesAndPreservesElevation()
{
    const auto controller = createController();

    controller->setLocationSourceText("Custom");
    controller->setElevationText("408.0");
    controller->setSelectedCityId("ch-zurich");

    QCOMPARE(controller->locationSourceText(), QString("City"));
    QCOMPARE(controller->selectedCityId(), QString("ch-zurich"));
    QCOMPARE(controller->selectedCityDisplayText(), QString("Zurich, Switzerland"));
    QCOMPARE(controller->latitudeText(), QString("47.376900"));
    QCOMPARE(controller->longitudeText(), QString("8.541700"));
    QCOMPARE(controller->elevationText(), QString("408.0"));
    QCOMPARE(controller->locationStatusText(), QString("Location: City - Zurich, Switzerland"));
}

void SkyContextControllerTests::manualCoordinateEditSwitchesToCustom()
{
    const auto controller = createController();

    controller->setSelectedCityId("ch-zurich");
    controller->setLongitudeText("8.600000");

    QCOMPARE(controller->locationSourceText(), QString("Custom"));
    QVERIFY(controller->selectedCityId().isEmpty());
    QVERIFY(controller->selectedCityDisplayText().isEmpty());
    QCOMPARE(controller->longitudeText(), QString("8.600000"));
    QCOMPARE(controller->locationStatusText(), QString("Location: Custom coordinates"));
}

void SkyContextControllerTests::invalidSavedCityFallsBackToCustom()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.locationSourceText = "City";
    snapshot.selectedCityId = "missing-city";
    snapshot.latitudeDeg = 12.345678;
    snapshot.longitudeDeg = 98.765432;
    snapshot.elevationMeters = 150.0;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->locationSourceText(), QString("Custom"));
    QVERIFY(controller->selectedCityId().isEmpty());
    QCOMPARE(controller->latitudeText(), QString("12.345678"));
    QCOMPARE(controller->longitudeText(), QString("98.765432"));
    QCOMPARE(controller->elevationText(), QString("150.0"));
}

void SkyContextControllerTests::defaultsLocationSourceByPositioningAvailability()
{
    const auto controller = createController();

    if (controller->locationSourceOptions().contains("Current Device")) {
        QCOMPARE(controller->locationSourceText(), QString("Current Device"));
    } else {
        QCOMPARE(controller->locationSourceText(), QString("Custom"));
    }
}

void SkyContextControllerTests::enablingUtcLocksDoesNotSnapTimelineToCurrentUtc()
{
    const auto controller = createController();
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);
    controller->setLive(false);

    const QDateTime startUtc = QDateTime::currentDateTimeUtc().addDays(-1).addSecs(-90);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));

    const qint64 beforeLockSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setUtcDateLocked(true);
    controller->setUtcTimeLocked(true);

    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), beforeLockSeconds);
}

void SkyContextControllerTests::lockedUtcDateBlocksManualDateEdits()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);

    const QDateTime startUtc = QDateTime::fromString("2026-01-02T03:04:05Z", Qt::ISODate);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));
    controller->setUtcDateLocked(true);

    controller->setUtcDateText("2026-01-09");

    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
}

void SkyContextControllerTests::lockedUtcTimeBlocksManualTimeEdits()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);

    const QDateTime startUtc = QDateTime::fromString("2026-01-02T03:04:05Z", Qt::ISODate);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));
    controller->setUtcTimeLocked(true);

    controller->setUtcTimeText("09:08:07");

    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
}

void SkyContextControllerTests::manualTimelineSteppingStillWorksWithUtcEditLocksEnabled()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->stepForward();
    const qint64 afterStepForwardSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->stepBackward();
    const qint64 afterStepBackwardSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();

    QCOMPARE(afterStepForwardSeconds - beforeSeconds, 60);
    QCOMPARE(afterStepBackwardSeconds, beforeSeconds);
}

void SkyContextControllerTests::livePlaybackUsesManualStepWhileCatchingUpWithUtcLocksEnabled()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    for (int index = 0; index < 5; ++index) {
        controller->stepBackward();
    }

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSeconds - beforeSeconds >= 120);

    controller->setLive(false);
}

void SkyContextControllerTests::livePlaybackUsesManualStepWhileCatchingUp()
{
    const auto controller = createController();
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = QDateTime::currentDateTimeUtc().addSecs(-5 * 60);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    controller->setLive(true);

    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSeconds - beforeSeconds >= 120);

    controller->setLive(false);
}

void SkyContextControllerTests::livePlaybackDoesNotOvershootCurrentUtcWhenCatchingUp()
{
    const auto controller = createController();
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = QDateTime::currentDateTimeUtc().addSecs(-30);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 currentTimelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    const qint64 currentUtcSeconds = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    QVERIFY(currentTimelineSeconds <= currentUtcSeconds);

    controller->setLive(false);
}

void SkyContextControllerTests::livePlaybackFallsBackToOneSecondTicksAfterCatchUp()
{
    const auto controller = createController();
    controller->setUtcDateLocked(false);
    controller->setUtcTimeLocked(false);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(4.0);

    const QDateTime startUtc = QDateTime::currentDateTimeUtc().addSecs(-2);
    controller->setUtcDateText(startUtc.toString("yyyy-MM-dd"));
    controller->setUtcTimeText(startUtc.toString("HH:mm:ss"));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterCatchUpSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    skyContextChangedSpy.clear();

    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterLiveTickSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(afterLiveTickSeconds - afterCatchUpSeconds, 1);

    controller->setLive(false);
}

void SkyContextControllerTests::goLiveNowJumpsToCurrentUtcAndEnablesLive()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);

    for (int index = 0; index < 5; ++index) {
        controller->stepBackward();
    }

    controller->goLiveNow();

    QVERIFY(controller->live());

    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    const qint64 currentUtcSeconds = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    QVERIFY(timelineSeconds >= currentUtcSeconds - 1);
    QVERIFY(timelineSeconds <= currentUtcSeconds + 1);
}

void SkyContextControllerTests::focusSearchTargetCentersBodyResult()
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

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerTests::focusSearchTargetCentersConstellationLabelResult()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            2.0,
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

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "hip_27989");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::any_of(
            controller->constellationLabelRefs().begin(),
            controller->constellationLabelRefs().end(),
            [](const SkyContextController::ConstellationLabelRef& labelRef) {
                return labelRef.first == "Orion";
            }
        )
    );

    QVERIFY(controller->focusSearchTarget("constellationLabel", "Orion"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("constellationLabel"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("Orion"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerTests::focusSearchTargetIgnoresInvalidTargets()
{
    const auto controller = createController();
    controller->setViewCenter(12.0, 123.0);

    QVERIFY(!controller->focusSearchTarget("body", "missing_target"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);

    QVERIFY(!controller->focusSearchTarget("unknown", "mars"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);
}

void SkyContextControllerTests::collapsingSearchToolbarClearsSelectedSearchTarget()
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

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));

    controller->setSearchToolbarCollapsed(true);
    QVERIFY(controller->selectedSearchTargetKind().isEmpty());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
}

QTEST_GUILESS_MAIN(SkyContextControllerTests)

#include "SkyContextControllerTests.moc"
