#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlComponentLoadTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void keyComponentsLoadWithoutWarnings_data();
    void keyComponentsLoadWithoutWarnings();
    void aboutWindowLoadsWithBuildMetadata();
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

    QTest::newRow("SearchToolbar")
        << QStringLiteral("SearchToolbar")
        << QStringLiteral(R"(
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

    QTest::newRow("TimelineToolbar")
        << QStringLiteral("TimelineToolbar")
        << QStringLiteral(R"(
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

    QTest::newRow("StatusFooter")
        << QStringLiteral("StatusFooter")
        << QStringLiteral(R"(
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

    QTest::newRow("PreferencesCatalogSection")
        << QStringLiteral("PreferencesCatalogSection")
        << QStringLiteral(R"(
            import QtQuick
            Item {
                width: 900
                height: 520
                SkySettingsDraft {
                    id: draft
                    skyContextController: skyContext
                    Component.onCompleted: resetFromContext()
                }
                PreferencesCatalogSection {
                    anchors.fill: parent
                    skyContextController: skyContext
                    settingsDraft: draft
                }
            }
        )");

    QTest::newRow("SkyOverlayLayer")
        << QStringLiteral("SkyOverlayLayer")
        << QStringLiteral(R"(
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

void QmlComponentLoadTests::aboutWindowLoadsWithBuildMetadata()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QStringLiteral("test-build"));
    engine.rootContext()->setContextProperty("skygateGitHash", QStringLiteral("test-git"));

    const QmlWarningScope warnings;
    auto object = createFileComponent(engine, QStringLiteral("AboutWindow.qml"));
    QVERIFY(object != nullptr);
    QCOMPARE(object->property("title").toString(), QStringLiteral("About SkyGate"));
    QVERIFY(firstObjectWithProperty(object.get(), "text", QStringLiteral("SkyGate")) != nullptr);
    QVERIFY(firstVisibleItemWithText(object.get(), QStringLiteral("Git test-git  |  Built test-build")) != nullptr);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlComponentLoadTests::malformedQmlEmitsCapturedWarning()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings(QmlWarningScope::Forwarding::Disabled);
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            missingProperty: true
        }
    )"), QStringLiteral("MalformedWarningTest.qml"));

    QVERIFY(object == nullptr);
    QVERIFY(!warnings.messages().isEmpty());
}

SKYGATE_QML_TEST_MAIN(QmlComponentLoadTests)

#include "QmlComponentLoadTests.moc"
