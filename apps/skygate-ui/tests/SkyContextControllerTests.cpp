#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyLogging.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SettingsTestFixture.hpp"
#include "SkySettingsStore.hpp"
#include "SkyTimeController.hpp"

#include "skygate/core/ITimeSource.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSignalSpy>
#include <QTimeZone>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#endif

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

QDateTime fixedNowUtc()
{
    return QDateTime(QDate(2026, 5, 6), QTime(9, 30, 0), QTimeZone::UTC);
}

class FakeTimeSource final : public skygate::core::ITimeSource {
public:
    explicit FakeTimeSource(const QDateTime& nowUtc = fixedNowUtc())
        : m_nowUtc(nowUtc.toUTC())
    {
    }

    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(m_nowUtc);
    }

    void setNowUtc(const QDateTime& nowUtc)
    {
        m_nowUtc = nowUtc.toUTC();
    }

    void advanceSeconds(const int seconds)
    {
        m_nowUtc = m_nowUtc.addSecs(seconds);
    }

private:
    QDateTime m_nowUtc;
};

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

SkyContextController::InitializationOptions controllerInitializationOptions(
    bool loadSettings,
    const skygate::core::ITimeSource* timeSource = nullptr
)
{
    SkyContextController::InitializationOptions initializationOptions;
    initializationOptions.loadSettings = loadSettings;
    initializationOptions.initializeLocation = false;
    initializationOptions.timeSource = timeSource;
    return initializationOptions;
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    bool loadSettings = false
);

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    bool loadSettings,
    const skygate::core::ITimeSource* timeSource
);

std::unique_ptr<SkyContextController> createControllerWithOptions(
    SkyContextController::InitializationOptions initializationOptions
);

std::unique_ptr<SkyContextController> createController(bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(std::move(starCatalog), std::move(ephemerisEngine), loadSettings);
}

std::unique_ptr<SkyContextController> createControllerWithTimeSource(
    const skygate::core::ITimeSource& timeSource,
    const bool loadSettings = false
)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        loadSettings,
        &timeSource
    );
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings,
    const skygate::core::ITimeSource* timeSource
)
{
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        controllerInitializationOptions(loadSettings, timeSource),
        nullptr
    );
}

std::unique_ptr<SkyContextController> createController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    const bool loadSettings
)
{
    return createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        loadSettings,
        nullptr
    );
}

std::unique_ptr<SkyContextController> createControllerWithOptions(
    SkyContextController::InitializationOptions initializationOptions
)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        initializationOptions,
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
    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller.setLatitudeText("47.3769");
    controller.setLongitudeText("8.5417");
    controller.setElevationText("408.0");
}

std::unique_ptr<SkyContextController> createSingleBodyController(
    const std::string& id = "demo_target",
    const std::string& displayName = "Demo Target",
    const skygate::core::ITimeSource* timeSource = nullptr
)
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            id,
            displayName,
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
    });
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    auto controller = createController(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        false,
        timeSource
    );
    configureFocusTestContext(*controller);
    return controller;
}

double azimuthDifferenceDeg(const double lhs, const double rhs)
{
    const double difference = std::abs(lhs - rhs);
    return std::min(difference, 360.0 - difference);
}

#if SKYGATE_HAS_POSITIONING
class FakePositionSource final : public QGeoPositionInfoSource {
    Q_OBJECT

public:
    explicit FakePositionSource(QObject* parent = nullptr)
        : QGeoPositionInfoSource(parent)
    {
    }

    [[nodiscard]] PositioningMethods supportedPositioningMethods() const override
    {
        return SatellitePositioningMethods;
    }

    [[nodiscard]] int minimumUpdateInterval() const override
    {
        return 0;
    }

    [[nodiscard]] Error error() const override
    {
        return m_error;
    }

    [[nodiscard]] QGeoPositionInfo lastKnownPosition(
        bool fromSatellitePositioningMethodsOnly = false
    ) const override
    {
        (void)fromSatellitePositioningMethodsOnly;
        return m_lastPosition;
    }

    void startUpdates() override {}
    void stopUpdates() override {}

    void requestUpdate(const int timeoutMs = 0) override
    {
        m_lastTimeoutMs = timeoutMs;
        ++m_requestCount;
    }

    [[nodiscard]] int requestCount() const noexcept
    {
        return m_requestCount;
    }

    [[nodiscard]] int lastTimeoutMs() const noexcept
    {
        return m_lastTimeoutMs;
    }

    void publishPosition(
        const double latitudeDeg,
        const double longitudeDeg,
        const double altitudeMeters
    )
    {
        QGeoCoordinate coordinate(latitudeDeg, longitudeDeg, altitudeMeters);
        QGeoPositionInfo positionInfo(coordinate, fixedNowUtc());
        m_lastPosition = positionInfo;
        emit positionUpdated(positionInfo);
    }

    void publishInvalidPosition()
    {
        emit positionUpdated(QGeoPositionInfo());
    }

    void publishError(const Error error)
    {
        m_error = error;
        emit errorOccurred(error);
    }

private:
    Error m_error = NoError;
    QGeoPositionInfo m_lastPosition;
    int m_requestCount = 0;
    int m_lastTimeoutMs = 0;
};
#endif

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
    void currentDevicePositionSourceAppliesUpdates();
    void currentDevicePositionSourceReportsErrorsAndIgnoresInvalidUpdates();
    void defaultsThemeToBundledDefault();
    void restoresSavedThemeId();
    void invalidSavedThemeFallsBackToDefault();
    void setThemeIdUpdatesPaletteAndEmitsSignals();
    void loggingSettingsApplyAndPersist();
    void invalidSavedTimeZoneFallsBackToUtc();
    void restoresSavedOverlayLayerVisibility();
    void setUtcDateTimeTextAppliesAtomically();
    void setUtcDateTimeTextAcceptsBceInput();
    void bceAliasesResolveToTheSameInstant();
    void invalidUtcDateTimeInputLeavesTimelineUnchanged();
    void invalidYearZeroUtcDateTimeIsRejected();
    void manualUtcApplyPausesLivePlayback();
    void manualTimelineSteppingStillWorks();
    void manualTimelineSteppingCrossesBceBoundaryWithoutYearZero();
    void livePlaybackUsesManualStepWhileCatchingUp();
    void livePlaybackDoesNotOvershootCurrentUtcWhenCatchingUp();
    void livePlaybackFallsBackToOneSecondTicksAfterCatchUp();
    void goLiveNowJumpsToCurrentUtcAndEnablesLive();
    void restoresLiveSettingsAtCurrentUtc();
    void restoresPausedSettingsAtSavedUtc();
    void focusSearchTargetCentersBodyResult();
    void focusSearchTargetCentersConstellationLabelResult();
    void focusSearchTargetIgnoresInvalidTargets();
    void trackSearchTargetSetsLiveCurrentTimeAndCentersBody();
    void trackSearchTargetRejectsInvalidTargetsWithoutMutation();
    void trackedTargetRecentersOnStepAndManualPan();
    void staleTrackedTargetClearsAndAllowsViewCenterChanges();
    void focusSearchTargetClearsTrackingForDifferentTarget();
    void clearingTrackedTargetPreservesSelectedSearchTarget();
    void collapsingSearchToolbarClearsSelectedSearchTarget();
    void nightConditionsPopulateAndRefreshForValidObserver();
    void failedDeepSkyCatalogDownloadKeepsCountLabel();
    void restoresCachedCatalogConstellationCount();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkygateUiControllerTests")));
}

void SkyContextControllerTests::init()
{
    skygate::ui::SkyLogging::uninstall();
    m_settings.clearSettings();
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

    controller->zoomViewByScaleDelta(200.0);
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

void SkyContextControllerTests::currentDevicePositionSourceAppliesUpdates()
{
#if SKYGATE_HAS_POSITIONING
    FakePositionSource positionSource;
    auto controller = createControllerWithOptions({
        .loadSettings = false,
        .initializeLocation = false,
        .positionSource = &positionSource,
        .requestLocationPermission = false
    });
    QVERIFY(controller != nullptr);
    QSignalSpy skyContextSpy(controller.get(), &SkyContextController::skyContextChanged);
    QSignalSpy locationStatusSpy(
        controller.get(),
        &SkyContextController::locationStatusTextChanged
    );

    controller->setLocationSourceText(QStringLiteral("Current Device"));
    controller->refreshCurrentLocation();

    QCOMPARE(controller->locationSourceText(), QString("Current Device"));
    QCOMPARE(positionSource.requestCount(), 1);
    QCOMPARE(
        positionSource.lastTimeoutMs(),
        skygate::ui::internal::SkyContextControllerConstants::kLocationUpdateTimeoutMs
    );
    QCOMPARE(controller->locationStatusText(), QString("Location: Locating"));

    positionSource.publishPosition(51.5074, -0.1278, 35.0);
    QTRY_COMPARE(controller->latitudeText(), QString("51.507400"));
    QCOMPARE(controller->longitudeText(), QString("-0.127800"));
    QCOMPARE(controller->elevationText(), QString("35.0"));
    QCOMPARE(controller->locationStatusText(), QString("Location: Using current location"));
    QVERIFY(skyContextSpy.count() >= 1);
    QVERIFY(locationStatusSpy.count() >= 2);
#else
    const auto controller = createController();
    QVERIFY(!controller->locationSourceOptions().contains("Current Device"));
#endif
}

void SkyContextControllerTests::currentDevicePositionSourceReportsErrorsAndIgnoresInvalidUpdates()
{
#if SKYGATE_HAS_POSITIONING
    FakePositionSource positionSource;
    auto controller = createControllerWithOptions({
        .loadSettings = false,
        .initializeLocation = false,
        .positionSource = &positionSource,
        .requestLocationPermission = false
    });
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QStringLiteral("12.000000"));
    controller->setLongitudeText(QStringLiteral("34.000000"));
    controller->setElevationText(QStringLiteral("56.0"));

    controller->setLocationSourceText(QStringLiteral("Current Device"));
    controller->refreshCurrentLocation();
    positionSource.publishInvalidPosition();
    QCoreApplication::processEvents();

    QCOMPARE(controller->latitudeText(), QString("12.000000"));
    QCOMPARE(controller->longitudeText(), QString("34.000000"));
    QCOMPARE(controller->elevationText(), QString("56.0"));
    QCOMPARE(controller->locationStatusText(), QString("Location: Locating"));

    positionSource.publishError(QGeoPositionInfoSource::UpdateTimeoutError);
    QTRY_COMPARE(controller->locationStatusText(), QString("Location: Update failed"));

    positionSource.publishPosition(47.3769, 8.5417, 408.0);
    QTRY_COMPARE(controller->locationStatusText(), QString("Location: Using current location"));
    QCOMPARE(controller->latitudeText(), QString("47.376900"));
    QCOMPARE(controller->longitudeText(), QString("8.541700"));
    QCOMPARE(controller->elevationText(), QString("408.0"));
#else
    const auto controller = createController();
    controller->setLocationSourceText(QStringLiteral("Current Device"));
    QCOMPARE(controller->locationSourceText(), QString("Custom"));
#endif
}

void SkyContextControllerTests::defaultsThemeToBundledDefault()
{
    const auto controller = createController(true);

    QCOMPARE(controller->themeId(), QString("default"));
    QCOMPARE(
        controller->theme()->property("windowBackground").value<QColor>(),
        QColor("#171b30")
    );
}

void SkyContextControllerTests::restoresSavedThemeId()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.themeId = "night-vision";
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    QCOMPARE(controller->themeId(), QString("night-vision"));
    QCOMPARE(
        controller->theme()->property("windowBackground").value<QColor>(),
        QColor("#150707")
    );
}

void SkyContextControllerTests::invalidSavedThemeFallsBackToDefault()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.themeId = "missing-theme";
    QVERIFY(store.saveState(snapshot));

    QTest::ignoreMessage(
        QtWarningMsg,
        "Unknown theme id missing-theme - using default theme"
    );
    const auto controller = createController(true);
    QCOMPARE(controller->themeId(), QString("default"));
}

void SkyContextControllerTests::setThemeIdUpdatesPaletteAndEmitsSignals()
{
    const auto controller = createController();
    QSignalSpy controllerThemeSpy(controller.get(), &SkyContextController::themeChanged);
    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    QSignalSpy themePaletteSpy(controller->theme(), SIGNAL(themeChanged()));

    controller->setThemeId("night-vision");

    QCOMPARE(controller->themeId(), QString("night-vision"));
    QCOMPARE(controllerThemeSpy.count(), 1);
    QCOMPARE(themePaletteSpy.count(), 1);
    QCOMPARE(skyContextChangedSpy.count(), 1);
    QCOMPARE(
        controller->theme()->property("windowBackground").value<QColor>(),
        QColor("#150707")
    );
}

void SkyContextControllerTests::loggingSettingsApplyAndPersist()
{
    const QString logFilePath = m_settings.filePath(QStringLiteral("controller-skygate.log"));
    const auto controller = createController();
    QSignalSpy loggingSpy(controller.get(), &SkyContextController::loggingChanged);

    controller->setLogToTerminal(false);
    controller->setLogToFile(true);
    controller->setLogFilePath(logFilePath);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));

    QCOMPARE(controller->logToTerminal(), false);
    QCOMPARE(controller->logToFile(), true);
    QCOMPARE(controller->logFilePath(), logFilePath);
    QVERIFY(loggingSpy.count() >= 3);
    auto activeConfiguration = skygate::ui::SkyLogging::configuration();
    QCOMPARE(activeConfiguration.logToTerminal, false);
    QCOMPARE(activeConfiguration.logToFile, true);
    QCOMPARE(activeConfiguration.logFilePath, logFilePath);

    QVERIFY(controller->saveSettings());
    const auto restoredController = createController(true);
    QCOMPARE(restoredController->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QCOMPARE(restoredController->logToTerminal(), false);
    QCOMPARE(restoredController->logToFile(), true);
    QCOMPARE(restoredController->logFilePath(), logFilePath);
    activeConfiguration = skygate::ui::SkyLogging::configuration();
    QCOMPARE(activeConfiguration.logToTerminal, false);
    QCOMPARE(activeConfiguration.logToFile, true);
    QCOMPARE(activeConfiguration.logFilePath, logFilePath);
}

void SkyContextControllerTests::invalidSavedTimeZoneFallsBackToUtc()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.displayTimeZoneId = QStringLiteral("Skygate/InvalidZone");
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->timeController()->timeZoneId(), QString("UTC"));
}

void SkyContextControllerTests::restoresSavedOverlayLayerVisibility()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.overlayLayers.horizon = false;
    snapshot.overlayLayers.altAzGrid = false;
    snapshot.overlayLayers.constellationLines = false;
    snapshot.overlayLayers.constellationLabels = false;
    snapshot.overlayLayers.ecliptic = true;
    snapshot.overlayLayers.celestialEquator = true;
    snapshot.overlayLayers.circumpolarBoundary = true;
    snapshot.overlayLayers.solarSystemLabels = false;
    snapshot.overlayLayers.deepSkyObjects = false;
    snapshot.overlayLayers.deepSkyLabels = false;
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);
    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller->overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    QVERIFY(!overlayLayers->horizon());
    QVERIFY(!overlayLayers->altAzGrid());
    QVERIFY(!overlayLayers->constellationLines());
    QVERIFY(!overlayLayers->constellationLabels());
    QVERIFY(overlayLayers->ecliptic());
    QVERIFY(overlayLayers->celestialEquator());
    QVERIFY(overlayLayers->circumpolarBoundary());
    QVERIFY(!overlayLayers->solarSystemLabels());
    QVERIFY(!overlayLayers->deepSkyObjects());
    QVERIFY(!overlayLayers->deepSkyLabels());
}

void SkyContextControllerTests::setUtcDateTimeTextAppliesAtomically()
{
    const auto controller = createController();
    controller->setLive(false);

    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));
    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
    QCOMPARE(
        controllerUtcTime(*controller).toSecsSinceEpoch(),
        QDateTime::fromString("2026-01-02T03:04:05Z", Qt::ISODate).toSecsSinceEpoch()
    );
}

void SkyContextControllerTests::setUtcDateTimeTextAcceptsBceInput()
{
    const auto controller = createController();
    controller->setLive(false);

    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));
    QCOMPARE(controller->utcDateText(), QString("0044-03-15 BCE"));
    QCOMPARE(controller->utcTimeText(), QString("12:00:00"));
    QCOMPARE(controllerUtcTime(*controller).date(), QDate(-44, 3, 15));
}

void SkyContextControllerTests::bceAliasesResolveToTheSameInstant()
{
    const auto bcController = createController();
    bcController->setLive(false);
    QVERIFY(bcController->setUtcDateTimeText("0044-03-15 BC", "12:00:00"));

    const auto bceController = createController();
    bceController->setLive(false);
    QVERIFY(bceController->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    QCOMPARE(
        controllerUtcTime(*bcController).toSecsSinceEpoch(),
        controllerUtcTime(*bceController).toSecsSinceEpoch()
    );
}

void SkyContextControllerTests::invalidUtcDateTimeInputLeavesTimelineUnchanged()
{
    const auto controller = createController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(!controller->setUtcDateTimeText("2026-02-30", "09:08:07"));

    QCOMPARE(controller->utcDateText(), QString("2026-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), beforeSeconds);
}

void SkyContextControllerTests::invalidYearZeroUtcDateTimeIsRejected()
{
    const auto controller = createController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2026-01-02", "03:04:05"));

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(
        controller->validateUtcDateTimeText("0000-01-01", "00:00:00"),
        QString("Year 0000 is invalid. Use 0001 BCE for the year before 0001 CE.")
    );
    QVERIFY(!controller->setUtcDateTimeText("0000-01-01", "00:00:00"));
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), beforeSeconds);
}

void SkyContextControllerTests::manualUtcApplyPausesLivePlayback()
{
    const auto controller = createController();
    controller->setLive(true);

    QVERIFY(controller->setUtcDateTimeText("2026-01-09", "09:08:07"));
    QVERIFY(!controller->live());
    QCOMPARE(controller->utcDateText(), QString("2026-01-09"));
    QCOMPARE(controller->utcTimeText(), QString("09:08:07"));
}

void SkyContextControllerTests::manualTimelineSteppingStillWorks()
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

void SkyContextControllerTests::manualTimelineSteppingCrossesBceBoundaryWithoutYearZero()
{
    const auto controller = createController();
    controller->setLive(false);
    controller->setStepSeconds(60);
    QVERIFY(controller->setUtcDateTimeText("0001-12-31 BCE", "23:59:30"));

    controller->stepForward();
    QCOMPARE(controller->utcDateText(), QString("0001-01-01"));
    QCOMPARE(controller->utcTimeText(), QString("00:00:30"));

    controller->stepBackward();
    QCOMPARE(controller->utcDateText(), QString("0001-12-31 BCE"));
    QCOMPARE(controller->utcTimeText(), QString("23:59:30"));
}

void SkyContextControllerTests::livePlaybackUsesManualStepWhileCatchingUp()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-5 * 60);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

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
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(2.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-30);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    controller->setLive(true);
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 currentTimelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    const qint64 currentUtcSeconds = fixedNowUtc().toSecsSinceEpoch();
    QVERIFY(currentTimelineSeconds <= currentUtcSeconds);

    controller->setLive(false);
}

void SkyContextControllerTests::livePlaybackFallsBackToOneSecondTicksAfterCatchUp()
{
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);
    controller->setSpeedMultiplier(4.0);

    const QDateTime startUtc = fixedNowUtc().addSecs(-2);
    QVERIFY(controller->setUtcDateTimeText(
        startUtc.toString("yyyy-MM-dd"),
        startUtc.toString("HH:mm:ss")
    ));

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
    FakeTimeSource timeSource;
    const auto controller = createControllerWithTimeSource(timeSource);
    controller->setLive(false);
    controller->setStepSeconds(60);

    for (int index = 0; index < 5; ++index) {
        controller->stepBackward();
    }

    controller->goLiveNow();

    QVERIFY(controller->live());

    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());
}

void SkyContextControllerTests::restoresLiveSettingsAtCurrentUtc()
{
    FakeTimeSource timeSource;
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = true;
    snapshot.utcEpochSeconds =
        QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toSecsSinceEpoch();
    QVERIFY(store.saveState(snapshot));

    const auto controller = createControllerWithTimeSource(timeSource, true);

    QVERIFY(controller->live());
    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());
    QVERIFY(timelineSeconds != snapshot.utcEpochSeconds);
}

void SkyContextControllerTests::restoresPausedSettingsAtSavedUtc()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.live = false;
    snapshot.utcEpochSeconds =
        QDateTime::fromString("2000-01-01T00:00:00Z", Qt::ISODate).toSecsSinceEpoch();
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QVERIFY(!controller->live());
    QCOMPARE(controllerUtcTime(*controller).toSecsSinceEpoch(), snapshot.utcEpochSeconds);
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

void SkyContextControllerTests::trackSearchTargetSetsLiveCurrentTimeAndCentersBody()
{
    FakeTimeSource timeSource;
    const auto controller = createSingleBodyController("demo_target", "Demo Target", &timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2000-01-01", "00:00:00"));
    controller->setViewCenter(12.0, 123.0);

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));

    QVERIFY(controller->live());
    QVERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetKind(), QString("body"));
    QCOMPARE(controller->trackedTargetId(), QString("demo_target"));
    QCOMPARE(controller->trackedTargetDisplayText(), QString("Demo Target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));

    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerTests::trackSearchTargetRejectsInvalidTargetsWithoutMutation()
{
    const auto controller = createSingleBodyController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller->setViewCenter(12.0, 123.0);

    const QDateTime initialUtc = controllerUtcTime(*controller);
    QVERIFY(!controller->trackSearchTarget("body", "missing_target"));
    QVERIFY(!controller->hasTrackedTarget());
    QVERIFY(!controller->live());
    QVERIFY(controller->selectedSearchTargetKind().isEmpty());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
    QCOMPARE(controllerUtcTime(*controller), initialUtc);
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);

    QVERIFY(!controller->trackSearchTarget("constellationLabel", "Orion"));
    QVERIFY(!controller->hasTrackedTarget());
    QCOMPARE(controllerUtcTime(*controller), initialUtc);
}

void SkyContextControllerTests::trackedTargetRecentersOnStepAndManualPan()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    controller->panViewBy(15.0, -10.0);

    auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );

    controller->setStepSeconds(3600);
    controller->stepForward();

    snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerTests::staleTrackedTargetClearsAndAllowsViewCenterChanges()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    QVERIFY(controller->hasTrackedTarget());

    controller->loadCatalogPreset("bundled");

    QVERIFY(!controller->hasTrackedTarget());
    controller->setViewCenter(12.0, 123.0);
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);
}

void SkyContextControllerTests::focusSearchTargetClearsTrackingForDifferentTarget()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "tracked_target",
            "Tracked Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
        makeBody(
            "search_target",
            "Search Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.2,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 8.5,
                .declinationDeg = 12.5
            }
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    QVERIFY(controller->trackSearchTarget("body", "tracked_target"));
    QVERIFY(controller->hasTrackedTarget());

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* searchState = findStateById(snapshot, "search_target");
    QVERIFY(searchState != nullptr);

    QVERIFY(controller->focusSearchTarget("body", "search_target"));
    QVERIFY(!controller->hasTrackedTarget());
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("search_target"));
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - searchState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            searchState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerTests::clearingTrackedTargetPreservesSelectedSearchTarget()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    controller->clearTrackedTarget();

    QVERIFY(!controller->hasTrackedTarget());
    QVERIFY(controller->trackedTargetKind().isEmpty());
    QVERIFY(controller->trackedTargetId().isEmpty());
    QVERIFY(controller->trackedTargetDisplayText().isEmpty());
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));
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

void SkyContextControllerTests::nightConditionsPopulateAndRefreshForValidObserver()
{
    const auto controller = createController();
    configureFocusTestContext(*controller);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QVERIFY(controller->setUtcDateTimeText("2024-03-21", "12:00:00"));

    controller->refreshNightConditions();
    const QVariantMap initialConditions = controller->nightConditions();
    QVERIFY(initialConditions.value("valid").toBool());
    QCOMPARE(initialConditions.value("sunRows").toList().size(), 6);
    QVERIFY(initialConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(initialConditions.value("moonPhaseText").toString().contains("%"));
    QVERIFY(initialConditions.value("moonRiseText").toString() != "--");
    QVERIFY(initialConditions.value("moonSetText").toString() != "--");
    QVERIFY(
        QStringList({"sun", "twilight", "moon"}).contains(controller->nightConditionsIconKind())
    );

    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("UTC")));
    const QVariantMap utcConditions = controller->nightConditions();
    QVERIFY(utcConditions.value("valid").toBool());
    QVERIFY(utcConditions.value("locationText").toString().contains("UTC"));
    QVERIFY(!utcConditions.value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(utcConditions != initialConditions);

    QVERIFY(controller->setUtcDateTimeText("2024-03-22", "12:00:00"));
    controller->refreshNightConditions();
    const QVariantMap refreshedConditions = controller->nightConditions();
    QVERIFY(refreshedConditions.value("valid").toBool());
    QVERIFY(refreshedConditions != initialConditions);
}

void SkyContextControllerTests::failedDeepSkyCatalogDownloadKeepsCountLabel()
{
    const auto controller = createController();
    QSignalSpy infoSpy(
        controller.get(),
        &SkyContextController::deepSkyCatalogInfoTextChanged
    );

    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog download aborted: no valid source URLs"
    );
    controller->downloadDeepSkyCatalogFromUrl(QString());

    QCOMPARE(infoSpy.count(), 0);
    QCOMPARE(controller->deepSkyCatalogInfoText(), QString("Objects: 110"));
    QCOMPARE(controller->catalogStatusText(), QString("Catalog: Invalid URL"));
}

void SkyContextControllerTests::restoresCachedCatalogConstellationCount()
{
    QSettings settings;
    settings.setValue(
        "skyContext/catalogCachePath",
        m_settings.filePath(QStringLiteral("cached-hyg-catalog.csv"))
    );

    SkySettingsStore store;
    SkySettingsStore::StateSnapshot stateSnapshot;
    stateSnapshot.catalogPresetIndex = 1;
    QVERIFY(store.saveState(stateSnapshot));

    SkySettingsStore::CatalogCacheSnapshot cacheSnapshot;
    cacheSnapshot.sourceLabel = "HYG v4.2";
    cacheSnapshot.catalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Demo Star,6.7525,-16.7161,-1.46\n";
    cacheSnapshot.constellationLineRows = "hyg_1|hyg_1\n";
    cacheSnapshot.constellationLabelRows = "Demo|hyg_1\n";
    cacheSnapshot.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    cacheSnapshot.constellationCount = 88;
    QVERIFY(store.saveCatalogCache(cacheSnapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->catalogPresetIndex(), 1);
    QVERIFY(controller->catalogDatasetInfoText().contains("Constellations: 88"));
}

QTEST_GUILESS_MAIN(SkyContextControllerTests)

#include "SkyContextControllerTests.moc"
