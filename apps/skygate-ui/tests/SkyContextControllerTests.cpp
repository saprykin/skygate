#include <QtTest>

#include "SkyContextController.hpp"
#include "SkySettingsStore.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <limits>

namespace {

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

std::unique_ptr<SkyContextController> createController(bool loadSettings = false)
{
    auto starCatalog = createTestCatalog();
    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    return std::make_unique<SkyContextController>(
        std::move(starCatalog),
        std::move(ephemerisEngine),
        controllerInitializationOptions(loadSettings),
        nullptr
    );
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

QTEST_GUILESS_MAIN(SkyContextControllerTests)

#include "SkyContextControllerTests.moc"
