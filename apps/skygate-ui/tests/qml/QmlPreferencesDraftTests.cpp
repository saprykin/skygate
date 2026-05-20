#include "QmlPreferencesTestSupport.hpp"

class QmlPreferencesDraftTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void preferencesDraftApplyAndResetRoundTrip();
    void preferencesDraftCurrentDeviceApplyAndSyncWork();
    void saveAndRestoreState();
    void malformedSavedEphemerisSettingsFallBackInDraft();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesDraftTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesDraftTests")));
}

void QmlPreferencesDraftTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesDraftTests::preferencesDraftApplyAndResetRoundTrip()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QStringLiteral("47.000000"));
    controller->setLongitudeText(QStringLiteral("8.000000"));
    controller->setElevationText(QStringLiteral("400"));
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Europe/Zurich")));
    controller->overlayLayers()->setProperty("horizon", true);
    controller->setLogToTerminal(false);
    controller->setLogToFile(true);
    controller->setLogFilePath(QStringLiteral("/tmp/skygate-qml-test.log"));

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("preferences/PreferencesDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);

    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("47.000000"));
    QCOMPARE(object->property("longitudeText").toString(), QString("8.000000"));
    QCOMPARE(object->property("timeZoneId").toString(), QString("Europe/Zurich"));
    QVERIFY(object->property("timeZoneDisplayText").toString().contains("Europe/Zurich"));
    QCOMPARE(object->property("overlayHorizon").toBool(), true);
    QCOMPARE(object->property("logToTerminal").toBool(), false);
    QCOMPARE(object->property("logToFile").toBool(), true);
    QCOMPARE(object->property("logFilePath").toString(), QString("/tmp/skygate-qml-test.log"));

    object->setProperty("latitudeText", QStringLiteral("12.500000"));
    object->setProperty("longitudeText", QStringLiteral("34.500000"));
    object->setProperty("elevationText", QStringLiteral("88"));
    object->setProperty("locationSourceText", QStringLiteral("Custom"));
    object->setProperty("timeZoneId", QStringLiteral("Asia/Bishkek"));
    object->setProperty("projectionTypeText", QStringLiteral("AzimuthalEquidistant"));
    object->setProperty("overlayHorizon", false);
    object->setProperty("catalogPresetIndex", 2);
    object->setProperty("catalogUrlText", QStringLiteral("https://example.test/stars.csv"));
    object->setProperty("ephemerisEngineKindIndex", 1);
    object->setProperty("ephemerisCorrectionPresetIndex", 2);
    object->setProperty("ephemerisRefractionEnabled", false);
    object->setProperty("ephemerisAtmosphericPressureText", QStringLiteral("802.50"));
    object->setProperty("ephemerisAtmosphericTemperatureText", QStringLiteral("-6.0"));
    object->setProperty("ephemerisRelativeHumidityText", QStringLiteral("42.0"));
    object->setProperty("ephemerisWavelengthText", QStringLiteral("0.68"));
    object->setProperty("logToTerminal", true);
    object->setProperty("logToFile", false);
    object->setProperty("logFilePath", QStringLiteral("/tmp/skygate-qml-test-updated.log"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "applyToContext"));

    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    QCOMPARE(controller->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QCOMPARE(controller->projectionTypeText(), QString("AzimuthalEquidistant"));
    QCOMPARE(controller->overlayLayers()->property("horizon").toBool(), false);
    QCOMPARE(controller->catalogPresetIndex(), 2);
    QCOMPARE(controller->catalogUrlText(), QString("https://example.test/stars.csv"));
    QCOMPARE(controller->ephemerisEngineKindIndex(), 1);
    QCOMPARE(controller->ephemerisCorrectionPresetIndex(), 2);
    QCOMPARE(controller->ephemerisRefractionEnabled(), false);
    QCOMPARE(controller->ephemerisAtmosphericPressureText(), QString("802.50"));
    QCOMPARE(controller->ephemerisAtmosphericTemperatureText(), QString("-6.0"));
    QCOMPARE(controller->ephemerisRelativeHumidityText(), QString("42.0"));
    QCOMPARE(controller->ephemerisWavelengthText(), QString("0.68"));
    QCOMPARE(controller->logToTerminal(), true);
    QCOMPARE(controller->logToFile(), false);
    QCOMPARE(controller->logFilePath(), QString("/tmp/skygate-qml-test-updated.log"));

    object->setProperty("latitudeText", QStringLiteral("99.000000"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("12.500000"));
    QCOMPARE(object->property("catalogUrlText").toString(), QString("https://example.test/stars.csv"));
    QCOMPARE(object->property("ephemerisEngineKindIndex").toInt(), 1);
    QCOMPARE(object->property("ephemerisCorrectionPresetIndex").toInt(), 2);
    QCOMPARE(object->property("ephemerisRefractionEnabled").toBool(), false);
    QCOMPARE(object->property("ephemerisAtmosphericPressureText").toString(), QString("802.50"));
    QCOMPARE(object->property("timeZoneId").toString(), QString("Asia/Bishkek"));
    QCOMPARE(object->property("logFilePath").toString(), QString("/tmp/skygate-qml-test-updated.log"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesDraftTests::preferencesDraftCurrentDeviceApplyAndSyncWork()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(SKYGATE_QML_SOURCE_DIR));

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            property alias draft: draft
            property alias fakeController: fakeController
            QtObject {
                id: overlayLayers
                property bool horizon: true
                property bool altAzGrid: true
                property bool constellationLines: true
                property bool constellationLabels: true
                property bool ecliptic: false
                property bool celestialEquator: false
                property bool circumpolarBoundary: false
                property bool solarSystemLabels: true
                property bool deepSkyObjects: true
                property bool deepSkyLabels: true
            }
            QtObject {
                id: fakeTime
                property string timeZoneId: "Europe/Zurich"
                property string timeZoneDetailText: "CET - UTC+01:00"
                property var timeZoneCatalogModel: null
                function setTimeZoneId(value) {
                    timeZoneId = value
                    return true
                }
            }
            QtObject {
                id: fakeController
                property string latitudeText: "47.376900"
                property string longitudeText: "8.541700"
                property string elevationText: "408.0"
                property string locationSourceText: "Custom"
                property string selectedCityId: ""
                property string selectedCityDisplayText: ""
                property string projectionTypeText: "Stereographic"
                property string themeId: "default"
                property bool logToTerminal: true
                property bool logToFile: false
                property string logFilePath: "/tmp/fake-skygate.log"
                property int ephemerisEngineKindIndex: 0
                property int ephemerisCorrectionPresetIndex: 3
                property bool ephemerisRefractionEnabled: true
                property string ephemerisAtmosphericPressureText: "1013.25"
                property string ephemerisAtmosphericTemperatureText: "10.0"
                property string ephemerisRelativeHumidityText: "0.00"
                property string ephemerisWavelengthText: "0.55"
                property var time: fakeTime
                property var overlayLayers: overlayLayers
                property int lastCatalogPresetIndex: -1
                property string lastCatalogUrlText: ""
                property int lastDeepSkyCatalogPresetIndex: -1
                property string lastDeepSkyCatalogUrlText: ""
                property string lastLocationSourceText: ""
                property string lastElevationText: ""
                property int refreshCurrentLocationCount: 0
                function catalogPresetIndex() { return 0 }
                function catalogUrlText() { return "" }
                function deepSkyCatalogPresetIndex() { return 0 }
                function deepSkyCatalogUrlText() { return "" }
                function setLocationSourceText(value) {
                    lastLocationSourceText = value
                    locationSourceText = value
                }
                function setSelectedCityId(value) { selectedCityId = value }
                function setLatitudeText(value) { latitudeText = value }
                function setLongitudeText(value) { longitudeText = value }
                function setElevationText(value) {
                    lastElevationText = value
                    elevationText = value
                }
                function refreshCurrentLocation() { ++refreshCurrentLocationCount }
                function setProjectionTypeText(value) { projectionTypeText = value }
                function setThemeId(value) { themeId = value }
                function setLogToTerminal(value) { logToTerminal = value }
                function setLogToFile(value) { logToFile = value }
                function setLogFilePath(value) { logFilePath = value }
                function setCatalogPresetIndex(value) { lastCatalogPresetIndex = value }
                function setCatalogUrlText(value) { lastCatalogUrlText = value }
                function setDeepSkyCatalogPresetIndex(value) {
                    lastDeepSkyCatalogPresetIndex = value
                }
                function setDeepSkyCatalogUrlText(value) { lastDeepSkyCatalogUrlText = value }
                function setEphemerisEngineKindIndex(value) {
                    ephemerisEngineKindIndex = value
                }
                function setEphemerisCorrectionPresetIndex(value) {
                    ephemerisCorrectionPresetIndex = value
                }
                function setEphemerisRefractionEnabled(value) {
                    ephemerisRefractionEnabled = value
                }
                function setEphemerisAtmosphericPressureText(value) {
                    ephemerisAtmosphericPressureText = value
                }
                function setEphemerisAtmosphericTemperatureText(value) {
                    ephemerisAtmosphericTemperatureText = value
                }
                function setEphemerisRelativeHumidityText(value) {
                    ephemerisRelativeHumidityText = value
                }
                function setEphemerisWavelengthText(value) {
                    ephemerisWavelengthText = value
                }
            }
            PreferencesDraft {
                id: draft
                skyContextController: fakeController
            }
        }
    )"),
        QStringLiteral("PreferencesDraftCurrentDeviceBehaviorTest.qml")
    );
    QVERIFY(object != nullptr);
    QObject* draft = qvariant_cast<QObject*>(object->property("draft"));
    QObject* fakeController = qvariant_cast<QObject*>(object->property("fakeController"));
    QVERIFY(draft != nullptr);
    QVERIFY(fakeController != nullptr);

    QVERIFY(QMetaObject::invokeMethod(draft, "resetFromContext"));
    draft->setProperty("locationSourceText", QStringLiteral("Current Device"));
    draft->setProperty("elevationText", QStringLiteral("123.0"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(fakeController->property("lastLocationSourceText").toString(), QString("Current Device"));
    QCOMPARE(fakeController->property("lastElevationText").toString(), QString("123.0"));
    QCOMPARE(fakeController->property("refreshCurrentLocationCount").toInt(), 1);

    fakeController->setProperty("latitudeText", QStringLiteral("12.500000"));
    fakeController->setProperty("longitudeText", QStringLiteral("34.500000"));
    fakeController->setProperty("elevationText", QStringLiteral("88.0"));
    QVERIFY(QMetaObject::invokeMethod(draft, "syncDeviceLocationFromContext"));
    QCOMPARE(draft->property("latitudeText").toString(), QString("12.500000"));
    QCOMPARE(draft->property("longitudeText").toString(), QString("34.500000"));
    QCOMPARE(draft->property("elevationText").toString(), QString("88.0"));

    draft->setProperty("locationSourceText", QStringLiteral("Custom"));
    fakeController->setProperty("latitudeText", QStringLiteral("55.000000"));
    QVERIFY(QMetaObject::invokeMethod(draft, "syncDeviceLocationFromContext"));
    QCOMPARE(draft->property("latitudeText").toString(), QString("12.500000"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesDraftTests::saveAndRestoreState()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("preferences/PreferencesDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    object->setProperty("latitudeText", QStringLiteral("12.500000"));
    object->setProperty("longitudeText", QStringLiteral("34.500000"));
    object->setProperty("elevationText", QStringLiteral("88"));
    object->setProperty("locationSourceText", QStringLiteral("Custom"));
    object->setProperty("projectionTypeText", QStringLiteral("Perspective"));
    object->setProperty("themeId", QStringLiteral("night-vision"));
    object->setProperty("overlayHorizon", false);
    object->setProperty("overlayEcliptic", true);
    object->setProperty("catalogPresetIndex", 2);
    object->setProperty("catalogUrlText", QStringLiteral("file:///tmp/stars.csv"));
    object->setProperty("deepSkyCatalogPresetIndex", 2);
    object->setProperty("deepSkyCatalogUrlText", QStringLiteral("file:///tmp/dso.csv"));
    object->setProperty("ephemerisEngineKindIndex", 1);
    object->setProperty("ephemerisCorrectionPresetIndex", 1);
    object->setProperty("ephemerisRefractionEnabled", true);
    object->setProperty("ephemerisAtmosphericPressureText", QStringLiteral("900.00"));
    object->setProperty("ephemerisAtmosphericTemperatureText", QStringLiteral("4.5"));
    object->setProperty("ephemerisRelativeHumidityText", QStringLiteral("35.0"));
    object->setProperty("ephemerisWavelengthText", QStringLiteral("0.62"));
    object->setProperty("timeZoneId", QStringLiteral("Asia/Bishkek"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "applyToContext"));
    QVERIFY(controller->saveSettings());

    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QCOMPARE(restoredController->latitudeText(), QString("12.500000"));
    QCOMPARE(restoredController->longitudeText(), QString("34.500000"));
    QCOMPARE(restoredController->elevationText(), QString("88.0"));
    QCOMPARE(restoredController->locationSourceText(), QString("Custom"));
    QCOMPARE(restoredController->projectionTypeText(), QString("Perspective"));
    QCOMPARE(restoredController->themeId(), QString("night-vision"));
    QCOMPARE(restoredController->overlayLayerVisibility().horizon, false);
    QCOMPARE(restoredController->overlayLayerVisibility().ecliptic, true);
    QCOMPARE(restoredController->catalogPresetIndex(), 2);
    QCOMPARE(restoredController->catalogUrlText(), QString("file:///tmp/stars.csv"));
    QCOMPARE(restoredController->deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(restoredController->deepSkyCatalogUrlText(), QString("file:///tmp/dso.csv"));
    QCOMPARE(restoredController->ephemerisEngineKindIndex(), 1);
    QCOMPARE(restoredController->ephemerisCorrectionPresetIndex(), 1);
    QCOMPARE(restoredController->ephemerisRefractionEnabled(), true);
    QCOMPARE(restoredController->ephemerisAtmosphericPressureText(), QString("900.00"));
    QCOMPARE(restoredController->ephemerisAtmosphericTemperatureText(), QString("4.5"));
    QCOMPARE(restoredController->ephemerisRelativeHumidityText(), QString("35.0"));
    QCOMPARE(restoredController->ephemerisWavelengthText(), QString("0.62"));
    QCOMPARE(restoredController->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesDraftTests::malformedSavedEphemerisSettingsFallBackInDraft()
{
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/version"), skygate::ui::internal::SkyContextControllerConstants::kSettingsVersion
    );
    settings.setValue(QStringLiteral("skyContext/ephemeris/engineKind"), QStringLiteral("experimental"));
    settings.setValue(QStringLiteral("skyContext/ephemeris/correctionFlags"), QStringLiteral("full"));
    settings.setValue(QStringLiteral("skyContext/ephemeris/refractionEnabled"), QStringLiteral("sometimes"));
    settings.setValue(QStringLiteral("skyContext/ephemeris/atmosphericPressureHpa"), QStringLiteral("thin"));

    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("preferences/PreferencesDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));

    QCOMPARE(object->property("ephemerisEngineKindIndex").toInt(), 0);
    QCOMPARE(object->property("ephemerisCorrectionPresetIndex").toInt(), 3);
    QCOMPARE(object->property("ephemerisRefractionEnabled").toBool(), true);
    QCOMPARE(object->property("ephemerisAtmosphericPressureText").toString(), QString("1013.25"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesDraftTests)

#include "QmlPreferencesDraftTests.moc"
