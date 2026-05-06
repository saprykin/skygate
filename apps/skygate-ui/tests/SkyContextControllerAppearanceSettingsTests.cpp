#include "SkyContextControllerTestSupport.hpp"

class SkyContextControllerAppearanceSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void defaultsThemeToBundledDefault();
    void restoresSavedThemeId();
    void invalidSavedThemeFallsBackToDefault();
    void setThemeIdUpdatesPaletteAndEmitsSignals();
    void loggingSettingsApplyAndPersist();
    void invalidSavedTimeZoneFallsBackToUtc();
    void restoresSavedOverlayLayerVisibility();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerAppearanceSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerAppearanceSettingsTests")));
}

void SkyContextControllerAppearanceSettingsTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerAppearanceSettingsTests::defaultsThemeToBundledDefault()
{
    const auto controller = createController(true);

    QCOMPARE(controller->themeId(), QString("default"));
    QCOMPARE(
        controller->theme()->property("windowBackground").value<QColor>(),
        QColor("#171b30")
    );
}

void SkyContextControllerAppearanceSettingsTests::restoresSavedThemeId()
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

void SkyContextControllerAppearanceSettingsTests::invalidSavedThemeFallsBackToDefault()
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

void SkyContextControllerAppearanceSettingsTests::setThemeIdUpdatesPaletteAndEmitsSignals()
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

void SkyContextControllerAppearanceSettingsTests::loggingSettingsApplyAndPersist()
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

void SkyContextControllerAppearanceSettingsTests::invalidSavedTimeZoneFallsBackToUtc()
{
    SkySettingsStore store;
    SkySettingsStore::StateSnapshot snapshot;
    snapshot.displayTimeZoneId = QStringLiteral("Skygate/InvalidZone");
    QVERIFY(store.saveState(snapshot));

    const auto controller = createController(true);

    QCOMPARE(controller->timeController()->timeZoneId(), QString("UTC"));
}

void SkyContextControllerAppearanceSettingsTests::restoresSavedOverlayLayerVisibility()
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

QTEST_GUILESS_MAIN(SkyContextControllerAppearanceSettingsTests)

#include "SkyContextControllerAppearanceSettingsTests.moc"
