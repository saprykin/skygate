#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlInteractionLayerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void hoverClickDragAndWheelReachSceneAndController();

private:
    QmlSettingsFixture m_settings;
};

void QmlInteractionLayerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlInteractionLayerTests")));
}

void QmlInteractionLayerTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlInteractionLayerTests::hoverClickDragAndWheelReachSceneAndController()
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
            width: 640
            height: 420
            property alias fakeController: fakeController
            property alias fakeScene: fakeScene
            property alias interaction: interaction
            Rectangle {
                id: viewport
                anchors.fill: parent
            }
            QtObject {
                id: fakeController
                property int panCalls: 0
                property real lastPanAzimuth: 0
                property real lastPanAltitude: 0
                property int zoomWheelCalls: 0
                property int lastWheelDelta: 0
                property int selectedSearchClearCount: 0
                function panViewBy(deltaAzimuthDeg, deltaAltitudeDeg) {
                    ++panCalls
                    lastPanAzimuth = deltaAzimuthDeg
                    lastPanAltitude = deltaAltitudeDeg
                }
                function zoomViewByWheelDelta(wheelDeltaY) {
                    ++zoomWheelCalls
                    lastWheelDelta = wheelDeltaY
                }
                function clearSelectedSearchTarget() {
                    ++selectedSearchClearCount
                }
            }
            QtObject {
                id: fakeScene
                property bool selectReturn: true
                property int selectCalls: 0
                property real lastSelectX: -1
                property real lastSelectY: -1
                property real lastLabelX: -1
                property real lastLabelY: -1
                function objectLabelAt(x, y) {
                    lastLabelX = x
                    lastLabelY = y
                    return "label-" + Math.round(x) + "-" + Math.round(y)
                }
                function selectObjectAt(x, y) {
                    ++selectCalls
                    lastSelectX = x
                    lastSelectY = y
                    return selectReturn
                }
            }
            SkyInteractionLayer {
                id: interaction
                viewportItem: viewport
                skyContextController: fakeController
                skySceneModel: fakeScene
            }
        }
    )"), QStringLiteral("SkyInteractionLayerBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    QObject* fakeController = qvariant_cast<QObject*>(root->property("fakeController"));
    QObject* fakeScene = qvariant_cast<QObject*>(root->property("fakeScene"));
    QObject* interaction = qvariant_cast<QObject*>(root->property("interaction"));
    QVERIFY(fakeController != nullptr);
    QVERIFY(fakeScene != nullptr);
    QVERIFY(interaction != nullptr);

    ExposedQuickWindow exposed(root);
    QTest::mouseMove(exposed.window(), QPoint(80, 90));
    QTRY_COMPARE(interaction->property("hoveredObjectLabel").toString(), QString("label-80-90"));
    QCOMPARE(fakeScene->property("lastLabelX").toReal(), 80.0);
    QCOMPARE(fakeScene->property("lastLabelY").toReal(), 90.0);

    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(120, 130));
    QTRY_COMPARE(fakeScene->property("selectCalls").toInt(), 1);
    QCOMPARE(fakeScene->property("lastSelectX").toReal(), 120.0);
    QCOMPARE(fakeScene->property("lastSelectY").toReal(), 130.0);
    QCOMPARE(fakeController->property("selectedSearchClearCount").toInt(), 0);
    QCOMPARE(interaction->property("hoveredObjectLabel").toString(), QString("label-120-130"));

    fakeScene->setProperty("selectReturn", false);
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(140, 150));
    QTRY_COMPARE(fakeController->property("selectedSearchClearCount").toInt(), 1);

    const int selectCallsBeforeDrag = fakeScene->property("selectCalls").toInt();
    QTest::mousePress(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseMove(exposed.window(), QPoint(103, 103));
    QCOMPARE(fakeController->property("panCalls").toInt(), 0);
    QTest::mouseMove(exposed.window(), QPoint(120, 130));
    QTRY_COMPARE(fakeController->property("panCalls").toInt(), 1);
    QCOMPARE(fakeController->property("lastPanAzimuth").toReal(), -3.6);
    QCOMPARE(fakeController->property("lastPanAltitude").toReal(), -5.4);
    QTest::mouseRelease(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(120, 130));
    QCOMPARE(fakeScene->property("selectCalls").toInt(), selectCallsBeforeDrag);
    QCOMPARE(interaction->property("hoveredObjectLabel").toString(), QString());

    QWheelEvent wheelEvent(
        QPointF(160.0, 170.0),
        exposed.window()->mapToGlobal(QPoint(160, 170)),
        QPoint(),
        QPoint(0, 120),
        Qt::NoButton,
        Qt::NoModifier,
        Qt::NoScrollPhase,
        false
    );
    QCoreApplication::sendEvent(exposed.window(), &wheelEvent);
    QCoreApplication::processEvents();
    QTRY_COMPARE(fakeController->property("zoomWheelCalls").toInt(), 1);
    QCOMPARE(fakeController->property("lastWheelDelta").toInt(), 120);
    QCOMPARE(interaction->property("hoveredObjectLabel").toString(), QString("label-160-170"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlInteractionLayerTests)

#include "QmlInteractionLayerTests.moc"
