#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyOverlayLayerRendersPayloads();
    void skyOverlayLayerInspectorActionsAndAvoidanceWork();
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

void QmlRenderingTests::skyOverlayLayerInspectorActionsAndAvoidanceWork()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 520
            property alias scene: scene
            Rectangle {
                id: avoidPanel
                x: 100
                y: 100
                width: 180
                height: 80
                visible: true
            }
            QtObject {
                id: scene
                property real movedX: -1
                property real movedY: -1
                property bool unpinned: false
                property bool cleared: false
                property bool dragged: false
                property var overlayItems: [
                    { "kind": "label", "text": "Hidden Label", "x": 170, "y": 140 },
                    { "kind": "cardinal", "text": "N", "x": 170, "y": 140 }
                ]
                property var selectionMarker: ({ "kind": "searchSelection", "x": 5, "y": 5 })
                property var selectedObjectInspector: ({
                    "visible": true,
                    "title": "Sun",
                    "x": 2000,
                    "y": -50,
                    "pinned": false,
                    "targetKind": "body",
                    "targetId": "sun",
                    "fields": [
                        { "label": "Type", "value": "Star" }
                    ]
                })
                function moveSelectedObjectInspector(x, y) {
                    movedX = x
                    movedY = y
                    dragged = true
                    selectedObjectInspector = {
                        "visible": true,
                        "title": "Sun",
                        "x": x,
                        "y": y,
                        "pinned": true,
                        "targetKind": "body",
                        "targetId": "sun",
                        "fields": [
                            { "label": "Type", "value": "Star" }
                        ]
                    }
                }
                function setSelectedObjectInspectorPinned(pinned) {
                    unpinned = !pinned
                    selectedObjectInspector = {
                        "visible": true,
                        "title": "Sun",
                        "x": selectedObjectInspector.x,
                        "y": selectedObjectInspector.y,
                        "pinned": pinned,
                        "targetKind": "body",
                        "targetId": "sun",
                        "fields": [
                            { "label": "Type", "value": "Star" }
                        ]
                    }
                }
                function clearSelectedObjectInspector() {
                    cleared = true
                    selectedObjectInspector = ({})
                }
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
                avoidItems: [avoidPanel]
            }
        }
    )"), QStringLiteral("SkyOverlayLayerActionBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    QObject* scene = qvariant_cast<QObject*>(root->property("scene"));
    QVERIFY(scene != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;

    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Sun")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Hidden Label")) == nullptr);
    auto* cardinal = firstVisibleItemWithText(root, QStringLiteral("N"));
    QVERIFY(cardinal != nullptr);
    QVERIFY(cardinal->mapToScene(QPointF(0.0, 0.0)).y() > 180.0);
    QObject* inspectorObject = firstObjectWithMetaProperties(
        root,
        {"hasInspector", "inspectorPinned"}
    );
    auto* inspectorItem = qobject_cast<QQuickItem*>(inspectorObject);
    QVERIFY(inspectorItem != nullptr);
    QVERIFY(inspectorItem->x() + inspectorItem->width() <= root->width() - 8.0);
    QVERIFY(inspectorItem->y() >= 8.0);

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
    auto* selectionMarker = qobject_cast<QQuickItem*>(*selectionMarkerIt);
    QVERIFY(selectionMarker != nullptr);
    QCOMPARE(selectionMarker->x(), -12.0);
    QCOMPARE(selectionMarker->y(), -12.0);

    const auto centerButtons = invokableButtonsWithText(root, QStringLiteral("Center"));
    QCOMPARE(centerButtons.size(), 1);
    QVERIFY(activateControl(centerButtons.front()));
    QTRY_COMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("sun"));

    const auto trackButtons = invokableButtonsWithText(root, QStringLiteral("Track"));
    QCOMPARE(trackButtons.size(), 1);
    QVERIFY(activateControl(trackButtons.front()));
    QTRY_VERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetId(), QString("sun"));

    const auto untrackButtons = invokableButtonsWithText(root, QStringLiteral("Untrack"));
    QCOMPARE(untrackButtons.size(), 1);
    QVERIFY(activateControl(untrackButtons.front()));
    QTRY_VERIFY(!controller->hasTrackedTarget());

    const auto pinButtons = invokableButtonsWithText(root, QStringLiteral("Pin"));
    QCOMPARE(pinButtons.size(), 1);
    QVERIFY(activateControl(pinButtons.front()));
    QTRY_VERIFY(scene->property("movedX").toReal() >= 0.0);

    const auto unpinButtons = invokableButtonsWithText(root, QStringLiteral("Unpin"));
    QCOMPARE(unpinButtons.size(), 1);
    QVERIFY(activateControl(unpinButtons.front()));
    QTRY_VERIFY(scene->property("unpinned").toBool());

    const QPoint dragStart = inspectorItem->mapToScene(QPointF(30.0, 20.0)).toPoint();
    QTest::mousePress(exposed.window(), Qt::LeftButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(exposed.window(), dragStart + QPoint(-120, 80));
    QTest::mouseRelease(
        exposed.window(),
        Qt::LeftButton,
        Qt::NoModifier,
        dragStart + QPoint(-120, 80)
    );
    QTRY_VERIFY(scene->property("dragged").toBool());
    QVERIFY(scene->property("movedX").toReal() < root->width());
    QVERIFY(scene->property("movedY").toReal() >= 8.0);

    const auto closeButtons = invokableButtonsWithText(root, QStringLiteral("x"));
    QCOMPARE(closeButtons.size(), 1);
    QVERIFY(activateControl(closeButtons.front()));
    QTRY_VERIFY(scene->property("cleared").toBool());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
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
