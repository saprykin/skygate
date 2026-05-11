#include "QmlTestSupport.hpp"
#include "SkyTimeController.hpp"

using namespace skygate::ui::tests;

namespace {

bool horizontallyOverlaps(const QQuickItem& lhs, const QQuickItem& rhs)
{
    const QRectF lhsRect(lhs.mapToScene(QPointF(0.0, 0.0)), QSizeF(lhs.width(), lhs.height()));
    const QRectF rhsRect(rhs.mapToScene(QPointF(0.0, 0.0)), QSizeF(rhs.width(), rhs.height()));
    return lhsRect.right() > rhsRect.left() && rhsRect.right() > lhsRect.left();
}

}  // namespace

class QmlStatusFooterTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void dateTimeClickAndTrackingIndicatorInvokeControllerActions();
    void liveAndTrackingStateUpdateFooterPresentation();
    void rightFooterClusterKeepsNightTrackingAndTimeTargetsOrdered();

private:
    QmlSettingsFixture m_settings;
};

void QmlStatusFooterTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlStatusFooterTests")));
}

void QmlStatusFooterTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlStatusFooterTests::dateTimeClickAndTrackingIndicatorInvokeControllerActions()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->trackSearchTarget(QStringLiteral("body"), QStringLiteral("sun")));
    controller->clearSelectedSearchTarget();
    QVERIFY(controller->hasTrackedTarget());

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 48
            property bool dateClicked: false
            property alias scene: scene
            QtObject {
                id: scene
                property bool inspectorCleared: false
                property var selectedObjectInspector: ({})
                function clearSelectedObjectInspector() {
                    inspectorCleared = true
                    selectedObjectInspector = ({})
                }
            }
            StatusFooter {
                anchors.fill: parent
                skyContextController: skyContext
                sceneModel: scene
                dateTimePopupOpen: false
                nightConditionsPopupOpen: false
                onDateTimeClicked: root.dateClicked = true
            }
        }
    )"), QStringLiteral("StatusFooterBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    QObject* scene = qvariant_cast<QObject*>(root->property("scene"));
    QVERIFY(scene != nullptr);

    ExposedQuickWindow exposed(root);
    QQuickItem* timeItem = firstQuickItemWithObjectName(root, QStringLiteral("statusTimeMouse"));
    QVERIFY(timeItem != nullptr);
    QPointF timeCenter = timeItem->mapToScene(
        QPointF(timeItem->width() / 2.0, timeItem->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, timeCenter.toPoint());
    QTRY_VERIFY(root->property("dateClicked").toBool());

    QQuickItem* trackingItem = firstQuickItemWithObjectName(root, QStringLiteral("trackingMouse"));
    QVERIFY(trackingItem != nullptr);
    QPointF trackingCenter = trackingItem->mapToScene(
        QPointF(trackingItem->width() / 2.0, trackingItem->height() / 2.0)
    );
    QTest::mouseClick(
        exposed.window(),
        Qt::LeftButton,
        Qt::NoModifier,
        trackingCenter.toPoint()
    );
    QTRY_COMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("sun"));

    scene->setProperty(
        "selectedObjectInspector",
        QVariantMap {
            {QStringLiteral("visible"), true},
            {QStringLiteral("targetKind"), QStringLiteral("body")},
            {QStringLiteral("targetId"), QStringLiteral("sun")}
        }
    );
    QCoreApplication::processEvents();
    QTest::mouseClick(
        exposed.window(),
        Qt::LeftButton,
        Qt::NoModifier,
        trackingCenter.toPoint()
    );
    QTRY_VERIFY(scene->property("inspectorCleared").toBool());
    QTRY_VERIFY(controller->selectedSearchTargetId().isEmpty());
    QVERIFY(controller->hasTrackedTarget());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlStatusFooterTests::liveAndTrackingStateUpdateFooterPresentation()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QVERIFY(controller->setUtcDateTimeText(
        QStringLiteral("2026-05-06"),
        QStringLiteral("09:30:30")
    ));
    controller->setLive(false);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 260
            height: 48
            property bool popupOpen: true
            property alias scene: scene
            QtObject {
                id: scene
                property var selectedObjectInspector: ({})
                function clearSelectedObjectInspector() {
                    selectedObjectInspector = ({})
                }
            }
            StatusFooter {
                anchors.fill: parent
                skyContextController: skyContext
                sceneModel: scene
                dateTimePopupOpen: root.popupOpen
                nightConditionsPopupOpen: false
            }
        }
    )"), QStringLiteral("StatusFooterStateBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: Off")) != nullptr);
    QTRY_VERIFY(
        firstVisibleItemWithText(
            root,
            QStringLiteral("2026-05-06 15:30:30 UTC+06:00")
        ) != nullptr
    );
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("UTC")));
    QTRY_VERIFY(
        firstVisibleItemWithText(
            root,
            QStringLiteral("2026-05-06 09:30:30 UTC")
        ) != nullptr
    );
    auto* trackingIndicator = firstQuickItemWithObjectName(
        root,
        QStringLiteral("trackingIndicator")
    );
    QVERIFY(trackingIndicator != nullptr);
    QVERIFY(!trackingIndicator->isVisible());

    controller->setLive(true);
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: On")) != nullptr);

    QVERIFY(controller->trackSearchTarget(QStringLiteral("body"), QStringLiteral("sun")));
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: On")) != nullptr);
    QTRY_VERIFY(trackingIndicator->isVisible());

    controller->clearTrackedTarget();
    QTRY_VERIFY(!trackingIndicator->isVisible());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlStatusFooterTests::rightFooterClusterKeepsNightTrackingAndTimeTargetsOrdered()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->trackSearchTarget(QStringLiteral("body"), QStringLiteral("moon")));

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 420
            height: 48
            property bool nightClicked: false
            property alias scene: scene
            QtObject {
                id: scene
                property var selectedObjectInspector: ({})
                function clearSelectedObjectInspector() {
                    selectedObjectInspector = ({})
                }
            }
            StatusFooter {
                anchors.fill: parent
                skyContextController: skyContext
                sceneModel: scene
                dateTimePopupOpen: false
                nightConditionsPopupOpen: false
                onNightConditionsClicked: root.nightClicked = true
            }
        }
    )"), QStringLiteral("StatusFooterRightClusterTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QQuickItem* tracking = firstQuickItemWithObjectName(root, QStringLiteral("trackingMouse"));
    QQuickItem* night = firstQuickItemWithObjectName(root, QStringLiteral("nightConditionsMouse"));
    QQuickItem* time = firstQuickItemWithObjectName(root, QStringLiteral("statusTimeMouse"));
    QVERIFY(tracking != nullptr);
    QVERIFY(night != nullptr);
    QVERIFY(time != nullptr);
    QVERIFY(
        tracking->mapToScene(QPointF(0.0, 0.0)).x()
        < night->mapToScene(QPointF(0.0, 0.0)).x()
    );
    QVERIFY(
        night->mapToScene(QPointF(0.0, 0.0)).x()
        < time->mapToScene(QPointF(0.0, 0.0)).x()
    );
    QVERIFY(!horizontallyOverlaps(*tracking, *night));

    const QPointF nightCenter = night->mapToScene(
        QPointF(night->width() / 2.0, night->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, nightCenter.toPoint());
    QTRY_VERIFY(root->property("nightClicked").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlStatusFooterTests)

#include "QmlStatusFooterTests.moc"
