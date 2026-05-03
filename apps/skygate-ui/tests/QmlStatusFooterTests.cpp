#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

namespace {

QList<QQuickItem*> pointingHandItems(QObject* root)
{
    QList<QQuickItem*> items;
    const auto objects = objectTree(root);
    for (QObject* object : objects) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item != nullptr
            && item->isVisible()
            && object->property("cursorShape").toInt() == Qt::PointingHandCursor
        ) {
            items.push_back(item);
        }
    }

    std::sort(
        items.begin(),
        items.end(),
        [](const QQuickItem* lhs, const QQuickItem* rhs) {
            const QPointF lhsCenter = lhs->mapToScene(
                QPointF(lhs->width() / 2.0, lhs->height() / 2.0)
            );
            const QPointF rhsCenter = rhs->mapToScene(
                QPointF(rhs->width() / 2.0, rhs->height() / 2.0)
            );
            return lhsCenter.x() < rhsCenter.x();
        }
    );
    return items;
}

}  // namespace

class QmlStatusFooterTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void dateTimeClickAndTrackingIndicatorInvokeControllerActions();
    void liveAndTrackingStateUpdateFooterPresentation();

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
    QList<QQuickItem*> clickableItems = pointingHandItems(root);
    QVERIFY(clickableItems.size() >= 2);
    QQuickItem* timeItem = clickableItems.back();
    QPointF timeCenter = timeItem->mapToScene(
        QPointF(timeItem->width() / 2.0, timeItem->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, timeCenter.toPoint());
    QTRY_VERIFY(root->property("dateClicked").toBool());

    clickableItems = pointingHandItems(root);
    QVERIFY(clickableItems.size() >= 2);
    QQuickItem* trackingItem = clickableItems.front();
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
            }
        }
    )"), QStringLiteral("StatusFooterStateBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: Off")) != nullptr);
    QCOMPARE(pointingHandItems(root).size(), 1);

    controller->setLive(true);
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: On")) != nullptr);

    QVERIFY(controller->trackSearchTarget(QStringLiteral("body"), QStringLiteral("sun")));
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Live: On")) != nullptr);
    QTRY_VERIFY(pointingHandItems(root).size() >= 2);

    controller->clearTrackedTarget();
    QTRY_COMPARE(pointingHandItems(root).size(), 1);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlStatusFooterTests)

#include "QmlStatusFooterTests.moc"
