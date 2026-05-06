#include "QmlRenderingTestSupport.hpp"

class QmlOverlayRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyOverlayLayerRendersPayloads();
    void skyOverlayLayerInspectorActionsAndAvoidanceWork();

private:
    QmlSettingsFixture m_settings;
};

void QmlOverlayRenderingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlOverlayRenderingTests")));
}

void QmlOverlayRenderingTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlOverlayRenderingTests::skyOverlayLayerRendersPayloads()
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
                        { "label": "Rise", "value": "2024-06-02 23:58:00 UTC" },
                        { "label": "Set", "value": "2024-06-03 04:12:00 UTC" },
                        { "label": "Culmination", "value": "2024-06-02 11:47:00 UTC at 72.4 deg" },
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
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Rise")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Set")) != nullptr);
    auto* riseValue = firstVisibleItemWithText(
        root,
        QStringLiteral("2024-06-02 23:58:00 UTC")
    );
    QVERIFY(riseValue != nullptr);
    QCOMPARE(riseValue->property("text").toString().count(QStringLiteral("UTC")), 1);
    QVERIFY(!riseValue->property("truncated").toBool());
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Culmination")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("2024-06-02 11:47:00 UTC at 72.4 deg")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Andromeda Galaxy")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Vega")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("N")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Sirius")) != nullptr);

    auto* selectionMarker = firstQuickItemWithObjectName(
        root,
        QStringLiteral("searchSelectionMarker")
    );
    QVERIFY(selectionMarker != nullptr);
    QVERIFY(selectionMarker->isVisible());
    QCOMPARE(selectionMarker->width(), 34.0);
    QCOMPARE(selectionMarker->height(), 34.0);

    auto* inspectorItem = firstQuickItemWithObjectName(root, QStringLiteral("objectInspector"));
    QVERIFY(inspectorItem != nullptr);
    auto* hoverLabel = firstQuickItemWithObjectName(root, QStringLiteral("hoverObjectLabel"));
    QVERIFY(hoverLabel != nullptr);
    auto* vegaLabel = firstQuickItemWithObjectName(root, QStringLiteral("skyOverlayLabel_Vega"));
    QVERIFY(vegaLabel != nullptr);
    auto* cardinalLabel = firstQuickItemWithObjectName(
        root,
        QStringLiteral("cardinalOverlayLabel_N")
    );
    QVERIFY(cardinalLabel != nullptr);

    const QImage overlayImage = exposed.window()->grabWindow();
    QVERIFY(!overlayImage.isNull());
    QObject* theme = controller->theme();
    QVERIFY(theme != nullptr);
    QVERIFY2(
        renderingScenePointIsColorNear(
            *exposed.window(),
            overlayImage,
            selectionMarker->mapToScene(QPointF(
                selectionMarker->width() * 0.5,
                selectionMarker->height() * 0.5
            )),
            theme->property("selectionMarkerCenter").value<QColor>(),
            12
        ),
        "Search selection marker center did not paint with the expected highlight color"
    );
    QVERIFY2(
        renderingItemRegionContainsColorNear(
            *exposed.window(),
            overlayImage,
            *selectionMarker,
            theme->property("selectionMarkerBorder").value<QColor>(),
            16
        ),
        "Search selection marker ring did not paint with the expected border color"
    );
    QVERIFY2(
        renderingItemRegionContainsColorNear(
            *exposed.window(),
            overlayImage,
            *inspectorItem,
            theme->property("toolbarDropdownBackground").value<QColor>(),
            4
        ),
        "Object inspector region did not paint its panel background"
    );
    QVERIFY2(
        renderingItemRegionContainsColorNear(
            *exposed.window(),
            overlayImage,
            *hoverLabel,
            theme->property("overlayTooltipBorder").value<QColor>(),
            24
        ),
        "Hover tooltip region did not paint its border"
    );
    QVERIFY2(
        renderingItemRegionContainsColorNear(
            *exposed.window(),
            overlayImage,
            *vegaLabel,
            theme->property("overlayLabelText").value<QColor>(),
            24
        ),
        "Sky overlay label region did not paint its label/border color"
    );
    QVERIFY2(
        renderingItemRegionContainsColorNear(
            *exposed.window(),
            overlayImage,
            *cardinalLabel,
            theme->property("overlayLabelText").value<QColor>(),
            24
        ),
        "Cardinal overlay label region did not paint its label/border color"
    );
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlOverlayRenderingTests::skyOverlayLayerInspectorActionsAndAvoidanceWork()
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

SKYGATE_QML_TEST_MAIN(QmlOverlayRenderingTests)

#include "QmlOverlayRenderingTests.moc"
