#include "support/QmlTestSupport.hpp"

#include <QColor>

using namespace skygate::ui::tests;

namespace {

QColor themeColor(const QObject* theme, const char* propertyName)
{
    return theme->property(propertyName).value<QColor>();
}

QColor checkIndicatorColor(QObject* checkBox)
{
    QObject* indicator = qvariant_cast<QObject*>(checkBox->property("indicator"));
    return indicator != nullptr ? indicator->property("color").value<QColor>() : QColor();
}

}  // namespace

class QmlComponentLoadTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void keyComponentsLoadWithoutWarnings_data();
    void keyComponentsLoadWithoutWarnings();
    void preferencesWindowUsesSuppliedControllerTheme();
    void preferenceControlUsesSuppliedTheme();
    void aboutWindowLoadsWithBuildMetadata();
    void aboutWindowClosesFromButtonsAndShortcuts();
    void malformedQmlEmitsCapturedWarning();

private:
    QmlSettingsFixture m_settings;
};

void QmlComponentLoadTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlComponentLoadTests")));
}

void QmlComponentLoadTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlComponentLoadTests::keyComponentsLoadWithoutWarnings_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("source");

    QTest::newRow("SearchToolbar") << QStringLiteral("SearchToolbar") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 120
                SearchToolbar {
                    anchors.fill: parent
                    skyContextController: skyContext
                }
            }
        )");

    QTest::newRow("SkyAppMenuButton") << QStringLiteral("SkyAppMenuButton") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 120
                height: 80
                SkyAppMenuButton {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    theme: skyContext.theme
                }
            }
        )");

    QTest::newRow("TimelineToolbar") << QStringLiteral("TimelineToolbar") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 120
                TimelineToolbar {
                    anchors.fill: parent
                    skyContextController: skyContext
                }
            }
        )");

    QTest::newRow("StatusFooter") << QStringLiteral("StatusFooter") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 80
                StatusFooter {
                    anchors.fill: parent
                    skyContextController: skyContext
                    sceneModel: skyScene
                    dateTimePopupOpen: false
                }
            }
        )");

    QTest::newRow("StatusDegradationPopup") << QStringLiteral("StatusDegradationPopup") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                StatusDegradationPopup {
                    anchors.fill: parent
                    skyContextController: skyContext
                    sceneModel: skyScene
                }
            }
        )");

    QTest::newRow("PreferencesCatalogSection") << QStringLiteral("PreferencesCatalogSection") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                PreferencesCatalogSection {
                    anchors.fill: parent
                    skyContextController: skyContext
                }
            }
        )");

    QTest::newRow("PreferencesEngineSection") << QStringLiteral("PreferencesEngineSection") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                PreferencesDraft {
                    id: draft
                    skyContextController: skyContext
                    Component.onCompleted: resetFromContext()
                }
                PreferencesEngineSection {
                    anchors.fill: parent
                    skyContextController: skyContext
                    preferencesDraft: draft
                }
            }
        )");

    QTest::newRow("PreferencesGeneralSection") << QStringLiteral("PreferencesGeneralSection") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                PreferencesDraft {
                    id: draft
                    skyContextController: skyContext
                    Component.onCompleted: resetFromContext()
                }
                PreferencesGeneralSection {
                    theme: skyContext.theme
                    anchors.fill: parent
                    preferencesDraft: draft
                }
            }
        )");

    QTest::newRow("PreferencesSkySection") << QStringLiteral("PreferencesSkySection") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                PreferencesDraft {
                    id: draft
                    skyContextController: skyContext
                    Component.onCompleted: resetFromContext()
                }
                PreferencesSkySection {
                    theme: skyContext.theme
                    anchors.fill: parent
                    preferencesDraft: draft
                }
            }
        )");

    QTest::newRow("PreferencesTimeZonePicker") << QStringLiteral("PreferencesTimeZonePicker") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 120
                PreferencesTimeZonePicker {
                    theme: skyContext.theme
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    catalogModel: skyContext.time.timeZoneCatalogModel
                    selectedTimeZoneId: skyContext.time.timeZoneId
                    selectedText: skyContext.time.timeZoneId + " - " + skyContext.time.timeZoneDetailText
                }
            }
        )");

    QTest::newRow("SkyOverlayLayer") << QStringLiteral("SkyOverlayLayer") << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                QtObject {
                    id: scene
                    property var overlayItems: [
                        { "kind": "label", "text": "Vega", "x": 120, "y": 150 }
                    ]
                    property var selectionMarker: ({ "kind": "searchSelection", "x": 180, "y": 180 })
                    property var selectedObjectInspector: ({})
                    function moveSelectedObjectInspector(x, y) {}
                    function setSelectedObjectInspectorPinned(pinned) {}
                    function clearSelectedObjectInspector() {}
                }
                QtObject {
                    id: interaction
                    property string hoveredObjectLabel: ""
                    property real hoverX: 0
                    property real hoverY: 0
                }
                SkyOverlayLayer {
                    anchors.fill: parent
                    sceneModel: scene
                    interactionLayer: interaction
                    avoidItems: []
                }
            }
        )");
}

void QmlComponentLoadTests::keyComponentsLoadWithoutWarnings()
{
    QFETCH(QString, name);
    QFETCH(QString, source);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);

    QQmlEngine engine;
    setupEngine(engine, *controller, sceneModel.get());

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, source, name + QStringLiteral("Test.qml"));

    QVERIFY(object != nullptr);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::preferencesWindowUsesSuppliedControllerTheme()
{
    auto rootController = makeController();
    QVERIFY(rootController != nullptr);
    auto suppliedController = makeController();
    QVERIFY(suppliedController != nullptr);
    rootController->setThemeId(QStringLiteral("default"));
    suppliedController->setThemeId(QStringLiteral("night-vision"));

    QQmlEngine engine;
    setupEngine(engine, *rootController);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("windows/PreferencesWindow.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(suppliedController.get())}}
    );
    QVERIFY(object != nullptr);
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window != nullptr);

    QObject* suppliedTheme = suppliedController->theme();
    QObject* rootTheme = rootController->theme();
    QVERIFY(suppliedTheme != nullptr);
    QVERIFY(rootTheme != nullptr);
    QCOMPARE(qvariant_cast<QObject*>(window->property("theme")), suppliedTheme);

    QCOMPARE(window->color(), themeColor(suppliedTheme, "windowBackground"));
    QVERIFY(window->color() != themeColor(rootTheme, "windowBackground"));

    QObject* terminalCheckBox = firstObjectWithObjectName(window, QStringLiteral("logToTerminalCheckBox"));
    QVERIFY(terminalCheckBox != nullptr);
    QCOMPARE(checkIndicatorColor(terminalCheckBox), themeColor(suppliedTheme, "checkBackground"));
    QVERIFY(checkIndicatorColor(terminalCheckBox) != themeColor(rootTheme, "checkBackground"));

    QObject* engineStatusLabel =
        firstObjectWithObjectName(window, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QVERIFY(engineStatusLabel != nullptr);
    QCOMPARE(engineStatusLabel->property("color").value<QColor>(), themeColor(suppliedTheme, "listItemPrimaryText"));

    suppliedController->setThemeId(QStringLiteral("default"));
    QTRY_COMPARE(window->color(), themeColor(suppliedTheme, "windowBackground"));
    QTRY_COMPARE(checkIndicatorColor(terminalCheckBox), themeColor(suppliedTheme, "checkBackground"));
    QTRY_COMPARE(
        engineStatusLabel->property("color").value<QColor>(), themeColor(suppliedTheme, "listItemPrimaryText")
    );

    rootController->setThemeId(QStringLiteral("night-vision"));
    QCoreApplication::processEvents();
    QCOMPARE(window->color(), themeColor(suppliedTheme, "windowBackground"));
    QCOMPARE(checkIndicatorColor(terminalCheckBox), themeColor(suppliedTheme, "checkBackground"));
    QVERIFY(window->color() != themeColor(rootTheme, "windowBackground"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::preferenceControlUsesSuppliedTheme()
{
    auto rootController = makeController();
    QVERIFY(rootController != nullptr);
    auto suppliedController = makeController();
    QVERIFY(suppliedController != nullptr);
    rootController->setThemeId(QStringLiteral("night-vision"));
    suppliedController->setThemeId(QStringLiteral("default"));

    QQmlEngine engine;
    setupEngine(engine, *rootController);
    engine.rootContext()->setContextProperty("suppliedTheme", suppliedController->theme());

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            width: 320
            height: 80
            PreferencesTextField {
                objectName: "suppliedThemeField"
                anchors.fill: parent
                theme: suppliedTheme
                placeholderText: "Type here"
            }
        }
    )"),
        QStringLiteral("PreferencesTextFieldSuppliedThemeTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    auto* field = firstQuickItemWithObjectName(root, QStringLiteral("suppliedThemeField"));
    QVERIFY(field != nullptr);

    QObject* suppliedTheme = suppliedController->theme();
    QObject* rootTheme = rootController->theme();
    QVERIFY(suppliedTheme != nullptr);
    QVERIFY(rootTheme != nullptr);
    QCOMPARE(field->property("color").value<QColor>(), themeColor(suppliedTheme, "inputText"));
    QCOMPARE(
        field->property("placeholderTextColor").value<QColor>(), themeColor(suppliedTheme, "inputPlaceholderText")
    );
    QVERIFY(field->property("color").value<QColor>() != themeColor(rootTheme, "inputText"));

    suppliedController->setThemeId(QStringLiteral("night-vision"));
    QTRY_COMPARE(field->property("color").value<QColor>(), themeColor(suppliedTheme, "inputText"));
    QTRY_COMPARE(
        field->property("placeholderTextColor").value<QColor>(), themeColor(suppliedTheme, "inputPlaceholderText")
    );

    rootController->setThemeId(QStringLiteral("default"));
    QCoreApplication::processEvents();
    QCOMPARE(field->property("color").value<QColor>(), themeColor(suppliedTheme, "inputText"));
    QVERIFY(field->property("color").value<QColor>() != themeColor(rootTheme, "inputText"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::aboutWindowLoadsWithBuildMetadata()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QStringLiteral("test-build"));
    engine.rootContext()->setContextProperty("skygateGitHash", QStringLiteral("test-git"));

    const QmlWarningScope warnings;
    auto object = createFileComponent(engine, QStringLiteral("windows/AboutWindow.qml"));
    QVERIFY(object != nullptr);
    QCOMPARE(object->property("title").toString(), QStringLiteral("About SkyGate"));
    QVERIFY(firstObjectWithProperty(object.get(), "text", QStringLiteral("SkyGate")) != nullptr);
    QVERIFY(firstVisibleItemWithText(object.get(), QStringLiteral("Git test-git  |  Built test-build")) != nullptr);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::aboutWindowClosesFromButtonsAndShortcuts()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QStringLiteral("test-build"));
    engine.rootContext()->setContextProperty("skygateGitHash", QStringLiteral("test-git"));

    const QmlWarningScope warnings;
    auto object = createFileComponent(engine, QStringLiteral("windows/AboutWindow.qml"));
    QVERIFY(object != nullptr);
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window != nullptr);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_VERIFY(window->isVisible());
    QObject* closeButton = firstObjectWithObjectName(window, QStringLiteral("aboutCloseButton"));
    QVERIFY(closeButton != nullptr);
    QVERIFY(activateControl(closeButton));
    QTRY_VERIFY(!window->isVisible());

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_VERIFY(window->isVisible());
    QObject* closeIconButton = firstObjectWithObjectName(window, QStringLiteral("aboutCloseIconButton"));
    QVERIFY(closeIconButton != nullptr);
    QVERIFY(activateControl(closeIconButton));
    QTRY_VERIFY(!window->isVisible());

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_VERIFY(window->isVisible());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->isVisible());

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_VERIFY(window->isVisible());
    QTest::keySequence(window, QKeySequence::Close);
    QTRY_VERIFY(!window->isVisible());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::malformedQmlEmitsCapturedWarning()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings(QmlWarningScope::Forwarding::Disabled);
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            missingProperty: true
        }
    )"),
        QStringLiteral("MalformedWarningTest.qml")
    );

    QVERIFY(object == nullptr);
    QVERIFY(!warnings.messages().isEmpty());
}

SKYGATE_QML_TEST_MAIN(QmlComponentLoadTests)

#include "QmlComponentLoadTests.moc"
