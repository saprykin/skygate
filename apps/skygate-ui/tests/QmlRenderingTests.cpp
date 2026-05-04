#include "QmlTestSupport.hpp"

#include <optional>

#include <QDateTime>
#include <QTimeZone>

using namespace skygate::ui::tests;

class QmlRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyOverlayLayerRendersPayloads();
    void skyOverlayLayerInspectorActionsAndAvoidanceWork();
    void skyViewportItemRendersNonBlankScene();
    void skyViewportItemHandlesModelLifecycleGeometryAndFrameChanges();
    void skyViewportItemOverlayLayersAddExpectedGeometry_data();
    void skyViewportItemOverlayLayersAddExpectedGeometry();
    void skyViewportItemAstronomicalLayersAddExpectedGeometry_data();
    void skyViewportItemAstronomicalLayersAddExpectedGeometry();
    void mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds();

private:
    QmlSettingsFixture m_settings;
};

namespace {

QObject* renderingObjectWithWindowTitle(QObject* root, const QString& title)
{
    for (QObject* object : objectTree(root)) {
        if (qobject_cast<QWindow*>(object) != nullptr && object->property("title") == title) {
            return object;
        }
    }
    return nullptr;
}

bool renderingTriggerMenuItem(QObject* menuItem)
{
    if (menuItem == nullptr) {
        return false;
    }
    if (menuItem->metaObject()->indexOfMethod("trigger()") >= 0) {
        return QMetaObject::invokeMethod(menuItem, "trigger");
    }
    if (menuItem->metaObject()->indexOfSignal("triggered()") >= 0) {
        return QMetaObject::invokeMethod(menuItem, "triggered");
    }
    return false;
}

bool windowHasMultipleSampledColors(QQuickWindow& window)
{
    const QImage image = window.grabWindow();
    if (image.isNull()) {
        return false;
    }

    std::optional<QRgb> firstColor;
    for (int y = 0; y < image.height(); y += std::max(1, image.height() / 12)) {
        for (int x = 0; x < image.width(); x += std::max(1, image.width() / 12)) {
            const QRgb color = image.pixel(x, y);
            if (!firstColor.has_value()) {
                firstColor = color;
                continue;
            }
            if (color != firstColor.value()) {
                return true;
            }
        }
    }
    return false;
}

bool visibleQuickItemsFitWithinWindow(QQuickWindow& window, QString* failure)
{
    const QRectF windowRect(0.0, 0.0, window.width(), window.height());
    for (QObject* object : objectTree(&window)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item == nullptr
            || item->window() != &window
            || !item->isVisible()
            || item->width() <= 0.0
            || item->height() <= 0.0
        ) {
            continue;
        }

        const QRectF bounds(item->mapToScene(QPointF(0.0, 0.0)), item->size());
        const QRectF relaxedWindowRect = windowRect.adjusted(-1.0, -1.0, 1.0, 1.0);
        if (!relaxedWindowRect.contains(bounds)) {
            if (failure != nullptr) {
                *failure = QStringLiteral("%1 at [%2,%3 %4x%5] outside %6x%7")
                    .arg(QString::fromUtf8(item->metaObject()->className()))
                    .arg(bounds.x())
                    .arg(bounds.y())
                    .arg(bounds.width())
                    .arg(bounds.height())
                    .arg(window.width())
                    .arg(window.height());
            }
            return false;
        }
    }
    return true;
}

}  // namespace

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
                        { "label": "Rise / Set", "value": "2024-06-02 23:58:00 / 2024-06-03 04:12:00 UTC" },
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
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Rise / Set")) != nullptr);
    auto* riseSetValue = firstVisibleItemWithText(
        root,
        QStringLiteral("2024-06-02 23:58:00 / 2024-06-03 04:12:00 UTC")
    );
    QVERIFY(riseSetValue != nullptr);
    QCOMPARE(riseSetValue->property("text").toString().count(QStringLiteral("UTC")), 1);
    QVERIFY(!riseSetValue->property("truncated").toBool());
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("Culmination")) != nullptr);
    QVERIFY(firstVisibleItemWithText(root, QStringLiteral("2024-06-02 11:47:00 UTC at 72.4 deg")) != nullptr);
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
                        { "label": "Type", "value": "Star" },
                        { "label": "Rise / Set", "value": "Always above" },
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
                            { "label": "Rise / Set", "value": "Always above" },
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
                            { "label": "Rise / Set", "value": "Always above" },
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

void QmlRenderingTests::skyViewportItemHandlesModelLifecycleGeometryAndFrameChanges()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    TestSkyViewportItem viewport;
    QSignalSpy modelChangedSpy(&viewport, &SkyViewportItem::skySceneModelChanged);
    viewport.setWidth(640.0);
    viewport.setHeight(420.0);
    viewport.setSkySceneModel(sceneModel.get());
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(viewport.skySceneModel(), sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());
    QCOMPARE(sceneModel->preparedProjection()->params().viewportWidth, 640.0);
    QCOMPARE(sceneModel->preparedProjection()->params().viewportHeight, 420.0);

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int populatedNodeCount = nodeTreeSize(paintNode.get());
    const int populatedVertexCount = geometryVertexCount(paintNode.get());
    QVERIFY(populatedNodeCount > 3);
    QVERIFY(populatedVertexCount > 0);

    viewport.setWidth(320.0);
    viewport.setHeight(240.0);
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());
    QCOMPARE(sceneModel->preparedProjection()->params().viewportWidth, 320.0);
    QCOMPARE(sceneModel->preparedProjection()->params().viewportHeight, 240.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(geometryVertexCount(paintNode.get()) > 0);

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setProperty("altAzGrid", false);
    QCoreApplication::processEvents();
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int changedVertexCount = geometryVertexCount(paintNode.get());
    QVERIFY(changedVertexCount > 0);
    QVERIFY(changedVertexCount != populatedVertexCount);

    viewport.setSkySceneModel(nullptr);
    QCOMPARE(modelChangedSpy.count(), 2);
    QCOMPARE(viewport.skySceneModel(), nullptr);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QCOMPARE(nodeTreeSize(paintNode.get()), 3);
    QCOMPARE(geometryVertexCount(paintNode.get()), 0);

    auto secondController = makeController();
    QVERIFY(secondController != nullptr);
    auto secondSceneModel = makeSceneModel(*secondController);
    QVERIFY(secondSceneModel != nullptr);
    viewport.setSkySceneModel(secondSceneModel.get());
    QCOMPARE(modelChangedSpy.count(), 3);
    QCOMPARE(viewport.skySceneModel(), secondSceneModel.get());
    QTRY_VERIFY(secondSceneModel->preparedProjection().has_value());
    QCOMPARE(secondSceneModel->preparedProjection()->params().viewportWidth, 320.0);
    QCOMPARE(secondSceneModel->preparedProjection()->params().viewportHeight, 240.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(geometryVertexCount(paintNode.get()) > 0);
}

void QmlRenderingTests::skyViewportItemOverlayLayersAddExpectedGeometry_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("horizon") << QStringLiteral("horizon");
    QTest::newRow("alt-az-grid") << QStringLiteral("altAzGrid");
    QTest::newRow("celestial-equator") << QStringLiteral("celestialEquator");
    QTest::newRow("deep-sky-glyphs") << QStringLiteral("deepSkyObjects");
}

void QmlRenderingTests::skyViewportItemOverlayLayersAddExpectedGeometry()
{
    QFETCH(QString, propertyName);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setMagnitudeCutoff(8.0);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    for (const char* layerProperty : {
        "horizon",
        "altAzGrid",
        "ecliptic",
        "celestialEquator",
        "circumpolarBoundary",
        "deepSkyObjects",
    }) {
        QVERIFY(overlayLayers->setProperty(layerProperty, false));
    }
    QCoreApplication::processEvents();

    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int baselineVertexCount = geometryVertexCount(paintNode.get());

    const QByteArray layerPropertyName = propertyName.toUtf8();
    QVERIFY(overlayLayers->setProperty(layerPropertyName.constData(), true));
    QCoreApplication::processEvents();
    viewport.setWidth(761.0);
    viewport.setWidth(760.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int enabledVertexCount = geometryVertexCount(paintNode.get());

    QVERIFY2(
        enabledVertexCount > baselineVertexCount,
        qPrintable(QString("%1 did not add scene-graph geometry").arg(propertyName))
    );
}

void QmlRenderingTests::skyViewportItemAstronomicalLayersAddExpectedGeometry_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("labelText");
    QTest::addColumn<double>("latitudeDeg");
    QTest::addColumn<double>("longitudeDeg");
    QTest::addColumn<QDateTime>("utcTime");
    QTest::addColumn<double>("centerAltitudeDeg");
    QTest::addColumn<double>("centerAzimuthDeg");

    QTest::newRow("ecliptic-greenwich-equinox")
        << QStringLiteral("ecliptic")
        << QStringLiteral("Ecliptic")
        << 51.4769
        << 0.0
        << QDateTime(QDate(2026, 3, 20), QTime(10, 0), QTimeZone::UTC)
        << 30.0
        << 180.0;
    QTest::newRow("circumpolar-reykjavik")
        << QStringLiteral("circumpolarBoundary")
        << QStringLiteral("Circumpolar")
        << 64.1466
        << -21.9426
        << QDateTime(QDate(2026, 1, 15), QTime(0, 0), QTimeZone::UTC)
        << 45.0
        << 0.0;
}

void QmlRenderingTests::skyViewportItemAstronomicalLayersAddExpectedGeometry()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, labelText);
    QFETCH(double, latitudeDeg);
    QFETCH(double, longitudeDeg);
    QFETCH(QDateTime, utcTime);
    QFETCH(double, centerAltitudeDeg);
    QFETCH(double, centerAzimuthDeg);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QString::number(latitudeDeg, 'f', 4));
    controller->setLongitudeText(QString::number(longitudeDeg, 'f', 4));
    QVERIFY(controller->setUtcDateTimeText(
        utcTime.date().toString(QStringLiteral("yyyy-MM-dd")),
        utcTime.time().toString(QStringLiteral("HH:mm:ss"))
    ));
    controller->setViewCenter(centerAltitudeDeg, centerAzimuthDeg);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    for (const char* layerProperty : {
        "horizon",
        "altAzGrid",
        "ecliptic",
        "celestialEquator",
        "circumpolarBoundary",
        "deepSkyObjects",
    }) {
        QVERIFY(overlayLayers->setProperty(layerProperty, false));
    }
    QCoreApplication::processEvents();

    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int baselineVertexCount = geometryVertexCount(paintNode.get());
    const int baselineNodeCount = nodeTreeSize(paintNode.get());
    const int baselineOverlayItemCount = sceneModel->overlayItems().size();

    const QByteArray layerPropertyName = propertyName.toUtf8();
    QVERIFY(overlayLayers->setProperty(layerPropertyName.constData(), true));
    QCoreApplication::processEvents();
    viewport.setWidth(761.0);
    viewport.setWidth(760.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);

    QVERIFY2(
        geometryVertexCount(paintNode.get()) > baselineVertexCount,
        qPrintable(QString("%1 did not add scene-graph geometry").arg(propertyName))
    );
    QVERIFY(nodeTreeSize(paintNode.get()) >= baselineNodeCount);
    QVERIFY(sceneModel->overlayItems().size() > baselineOverlayItemCount);

    bool hasReferenceLabel = false;
    for (const QVariant& item : sceneModel->overlayItems()) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("text")).toString() == labelText) {
            hasReferenceLabel = true;
            break;
        }
    }
    QVERIFY2(hasReferenceLabel, qPrintable(QString("%1 label was not projected").arg(labelText)));
}

void QmlRenderingTests::mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    setupEngine(engine, *controller, sceneModel.get());
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));

    const QmlWarningScope warnings;
    engine.load(QUrl::fromLocalFile(qmlSourcePath(QStringLiteral("Main.qml"))));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    QVERIFY(rootWindow != nullptr);
    (void)QTest::qWaitForWindowExposed(rootWindow);

    for (const QSize size : {QSize(1100, 760), QSize(560, 640)}) {
        rootWindow->resize(size);
        QCoreApplication::processEvents();
        QTRY_VERIFY(windowHasMultipleSampledColors(*rootWindow));
        QString failure;
        bool itemsFit = false;
        for (int attempt = 0; attempt < 30 && !itemsFit; ++attempt) {
            failure.clear();
            itemsFit = visibleQuickItemsFitWithinWindow(*rootWindow, &failure);
            if (!itemsFit) {
                QTest::qWait(10);
                QCoreApplication::processEvents();
            }
        }
        QVERIFY2(itemsFit, qPrintable(failure));
    }

    QObject* aboutItem = firstObjectWithProperty(
        rootWindow,
        "text",
        QStringLiteral("&About SkyGate")
    );
    QVERIFY(aboutItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(renderingTriggerMenuItem(aboutItem));
    auto* aboutWindow = qobject_cast<QQuickWindow*>(renderingObjectWithWindowTitle(
        rootWindow,
        QStringLiteral("About SkyGate")
    ));
    QVERIFY(aboutWindow != nullptr);
    QTRY_VERIFY(aboutWindow->isVisible());
    QTRY_VERIFY(windowHasMultipleSampledColors(*aboutWindow));
    QString failure;
    QVERIFY2(visibleQuickItemsFitWithinWindow(*aboutWindow, &failure), qPrintable(failure));

    QObject* preferencesItem = firstObjectWithProperty(
        rootWindow,
        "text",
        QStringLiteral("&Preferences...")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(renderingTriggerMenuItem(preferencesItem));
    auto* preferencesWindow = qobject_cast<QQuickWindow*>(renderingObjectWithWindowTitle(
        rootWindow,
        QStringLiteral("Preferences")
    ));
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->isVisible());
    QTRY_VERIFY(windowHasMultipleSampledColors(*preferencesWindow));
    failure.clear();
    QVERIFY2(visibleQuickItemsFitWithinWindow(*preferencesWindow, &failure), qPrintable(failure));

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlRenderingTests)

#include "QmlRenderingTests.moc"
