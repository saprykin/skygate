#include "SkyContextControllerTestSupport.hpp"

class SkyContextControllerViewLocationTests final : public QObject {
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

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerViewLocationTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerViewLocationTests")));
}

void SkyContextControllerViewLocationTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerViewLocationTests::pinchZoomScaleDeltaAdjustsAndClampsFieldOfView()
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

void SkyContextControllerViewLocationTests::pinchZoomScaleDeltaIgnoresInvalidValues()
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

void SkyContextControllerViewLocationTests::selectingCityUpdatesCoordinatesAndPreservesElevation()
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

void SkyContextControllerViewLocationTests::manualCoordinateEditSwitchesToCustom()
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

void SkyContextControllerViewLocationTests::invalidSavedCityFallsBackToCustom()
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

void SkyContextControllerViewLocationTests::defaultsLocationSourceByPositioningAvailability()
{
    const auto controller = createController();

    if (controller->locationSourceOptions().contains("Current Device")) {
        QCOMPARE(controller->locationSourceText(), QString("Current Device"));
    } else {
        QCOMPARE(controller->locationSourceText(), QString("Custom"));
    }
}

void SkyContextControllerViewLocationTests::currentDevicePositionSourceAppliesUpdates()
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

void SkyContextControllerViewLocationTests::currentDevicePositionSourceReportsErrorsAndIgnoresInvalidUpdates()
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

QTEST_GUILESS_MAIN(SkyContextControllerViewLocationTests)

#include "SkyContextControllerViewLocationTests.moc"
