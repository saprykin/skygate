#include "QmlOverlayRenderingTestSupport.hpp"

class QmlSkyOverlayLayerInspectorInteractionTests final : public QmlOverlayRenderingTestBase {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyOverlayLayerInspectorActionsAndAvoidanceWork();
};

void QmlSkyOverlayLayerInspectorInteractionTests::initTestCase()
{
    QVERIFY(initializeOverlayRenderingSettings(
        QStringLiteral("QmlSkyOverlayLayerInspectorInteractionTests")
    ));
}

void QmlSkyOverlayLayerInspectorInteractionTests::init()
{
    resetOverlayRenderingSettings();
}

void QmlSkyOverlayLayerInspectorInteractionTests::skyOverlayLayerInspectorActionsAndAvoidanceWork()
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
                        { "label": "Type", "value": "Star" },
                        { "label": "Rise", "value": "Always above" },
                        { "label": "Set", "value": "Always above" },
                        { "label": "Culmination", "value": "2024-06-02 03:12:00 UTC at 84.0 deg" }
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
                            { "label": "Type", "value": "Star" },
                            { "label": "Rise", "value": "Always above" },
                            { "label": "Set", "value": "Always above" },
                            { "label": "Culmination", "value": "2024-06-02 03:12:00 UTC at 84.0 deg" }
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
                            { "label": "Type", "value": "Star" },
                            { "label": "Rise", "value": "Always above" },
                            { "label": "Set", "value": "Always above" },
                            { "label": "Culmination", "value": "2024-06-02 03:12:00 UTC at 84.0 deg" }
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
    auto* inspectorItem = firstQuickItemWithObjectName(root, QStringLiteral("objectInspector"));
    QVERIFY(inspectorItem != nullptr);
    QVERIFY(inspectorItem->x() + inspectorItem->width() <= root->width() - 8.0);
    QVERIFY(inspectorItem->y() >= 8.0);

    auto* selectionMarker = firstQuickItemWithObjectName(
        root,
        QStringLiteral("searchSelectionMarker")
    );
    QVERIFY(selectionMarker != nullptr);
    QCOMPARE(selectionMarker->x(), -12.0);
    QCOMPARE(selectionMarker->y(), -12.0);

    QObject* centerButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorCenterButton")
    );
    QVERIFY(centerButton != nullptr);
    QVERIFY(activateControl(centerButton));
    QTRY_COMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("sun"));

    QObject* trackButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorTrackButton")
    );
    QVERIFY(trackButton != nullptr);
    QVERIFY(activateControl(trackButton));
    QTRY_VERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetId(), QString("sun"));

    QObject* untrackButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorUntrackButton")
    );
    QVERIFY(untrackButton != nullptr);
    QVERIFY(activateControl(untrackButton));
    QTRY_VERIFY(!controller->hasTrackedTarget());

    QObject* pinButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorPinButton")
    );
    QVERIFY(pinButton != nullptr);
    QVERIFY(activateControl(pinButton));
    QTRY_VERIFY(scene->property("movedX").toReal() >= 0.0);

    QObject* unpinButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorUnpinButton")
    );
    QVERIFY(unpinButton != nullptr);
    QVERIFY(activateControl(unpinButton));
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

    QObject* closeButton = firstObjectWithObjectName(
        root,
        QStringLiteral("objectInspectorCloseButton")
    );
    QVERIFY(closeButton != nullptr);
    QVERIFY(activateControl(closeButton));
    QTRY_VERIFY(scene->property("cleared").toBool());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlSkyOverlayLayerInspectorInteractionTests)

#include "QmlSkyOverlayLayerInspectorInteractionTests.moc"
