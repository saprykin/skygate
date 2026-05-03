#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyOverlayLayerRendersPayloads();
    void skyViewportItemRendersNonBlankScene();

private:
    QmlSettingsFixture m_settings;
};

void QmlRenderingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlRenderingTests")));
}

void QmlRenderingTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlRenderingTests::skyOverlayLayerRendersPayloads()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 900
            height: 520
            QtObject {
                id: scene
                property real movedX: -1
                property real movedY: -1
                property bool unpinned: false
                property bool cleared: false
                property var overlayItems: [
                    { "kind": "label", "text": "Vega", "x": 120, "y": 160 },
                    { "kind": "cardinal", "text": "N", "x": 450, "y": 40 }
                ]
                property var selectionMarker: ({ "kind": "searchSelection", "x": 220, "y": 210 })
                property var selectedObjectInspector: ({
                    "visible": true,
                    "title": "M31",
                    "x": 260,
                    "y": 180,
                    "pinned": false,
                    "targetKind": "body",
                    "targetId": "messier_031",
                    "fields": [
                        { "label": "Type", "value": "Galaxy" },
                        { "label": "Source", "value": "OpenNGC" }
                    ],
                    "aliases": "Andromeda Galaxy"
                })
                function moveSelectedObjectInspector(x, y) {
                    movedX = x
                    movedY = y
                }
                function setSelectedObjectInspectorPinned(pinned) {
                    unpinned = !pinned
                }
                function clearSelectedObjectInspector() {
                    cleared = true
                }
            }
            QtObject {
                id: interaction
                property string hoveredObjectLabel: "Sirius"
                property real hoverX: 40
                property real hoverY: 50
            }
            SkyOverlayLayer {
                id: overlay
                anchors.fill: parent
                sceneModel: scene
                interactionLayer: interaction
                avoidItems: []
            }
        }
    )"), QStringLiteral("SkyOverlayLayerBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("M31")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Type")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Galaxy")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Andromeda Galaxy")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Vega")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("N")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Sirius")) != nullptr);

    const auto objects = objectTree(root);
    const auto selectionMarkerIt = std::find_if(
        objects.begin(),
        objects.end(),
        [](QObject* object) {
            auto* item = qobject_cast<QQuickItem*>(object);
            return item != nullptr
                && item->isVisible()
                && qFuzzyCompare(item->width(), 34.0)
                && qFuzzyCompare(item->height(), 34.0);
        }
    );
    QVERIFY(selectionMarkerIt != objects.end());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlRenderingTests::skyViewportItemRendersNonBlankScene()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    TestSkyViewportItem viewport;
    viewport.setWidth(640.0);
    viewport.setHeight(420.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->snapshotGeneration() > 0U);
    QTRY_VERIFY(!sceneModel->renderPointSpan().empty() || !sceneModel->renderLineSpan().empty());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(nodeTreeSize(paintNode.get()) > 3);
}

SKYGATE_QML_TEST_MAIN(QmlRenderingTests)

#include "QmlRenderingTests.moc"
