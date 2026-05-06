#include "QmlTestSupport.hpp"
#include "SkyTimeController.hpp"

using namespace skygate::ui::tests;

class QmlDateTimePopupTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void dateTimePopupValidatesAppliesAndCancels();
    void dateTimePopupNowOutsideClickAndEnterKeyWork();

private:
    QmlSettingsFixture m_settings;
};

void QmlDateTimePopupTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlDateTimePopupTests")));
}

void QmlDateTimePopupTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlDateTimePopupTests::dateTimePopupValidatesAppliesAndCancels()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Europe/Zurich")));

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 920
            height: 360
            StatusDateTimePopup {
                id: dateTimePopup
                anchors.fill: parent
                skyContextController: skyContext
                popupRightMargin: 8
                popupBottomMargin: 8
            }
            property alias popup: dateTimePopup
        }
    )"), QStringLiteral("DateTimePopupBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;

    QObject* popup = qvariant_cast<QObject*>(root->property("popup"));
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(firstVisibleItemContainingText(root, QStringLiteral("Europe/Zurich")) != nullptr);
    popup->setProperty("stagedDateText", QStringLiteral("bad-date"));
    popup->setProperty("stagedTimeText", QStringLiteral("12:00:00"));
    QVERIFY(QMetaObject::invokeMethod(popup, "applyChanges"));
    QVERIFY(!popup->property("errorText").toString().isEmpty());
    QVERIFY(popup->property("opened").toBool());

    popup->setProperty("stagedDateText", QStringLiteral("2024-01-02"));
    popup->setProperty("stagedTimeText", QStringLiteral("03:04:05"));
    QVERIFY(QMetaObject::invokeMethod(popup, "applyChanges"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(controller->timeController()->dateText(), QString("2024-01-02"));
    QCOMPARE(controller->timeController()->timeText(), QString("03:04:05"));
    QCOMPARE(controller->utcTimeText(), QString("02:04:05"));

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "cancel"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlDateTimePopupTests::dateTimePopupNowOutsideClickAndEnterKeyWork()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("UTC")));
    QVERIFY(controller->setUtcDateTimeText("2024-01-02", "03:04:05"));
    controller->setLive(false);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 920
            height: 360
            StatusDateTimePopup {
                id: dateTimePopup
                anchors.fill: parent
                skyContextController: skyContext
                popupRightMargin: 8
                popupBottomMargin: 8
            }
            property alias popup: dateTimePopup
        }
    )"), QStringLiteral("DateTimePopupEdgeBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QObject* popup = qvariant_cast<QObject*>(root->property("popup"));
    QVERIFY(popup != nullptr);

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("stagedDateText").toString(), QString("2024-01-02"));
    QCOMPARE(popup->property("stagedTimeText").toString(), QString("03:04:05"));
    QObject* applyButton = firstObjectWithObjectName(
        root,
        QStringLiteral("dateTimePopupApplyButton")
    );
    QVERIFY(applyButton != nullptr);
    QVERIFY(applyButton->property("enabled").toBool());
    popup->setProperty("stagedDateText", QString());
    QCoreApplication::processEvents();
    QTRY_VERIFY(!applyButton->property("enabled").toBool());

    popup->setProperty("stagedDateText", QStringLiteral("2025-05-06"));
    popup->setProperty("stagedTimeText", QStringLiteral("07:08:09"));
    auto* timeInput = firstQuickItemWithObjectName(
        root,
        QStringLiteral("dateTimePopupTimeField")
    );
    QVERIFY(timeInput != nullptr);
    QVERIFY(QMetaObject::invokeMethod(timeInput, "forceActiveFocus"));
    QTest::keyClick(exposed.window(), Qt::Key_Return);
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(controller->timeController()->dateText(), QString("2025-05-06"));
    QCOMPARE(controller->timeController()->timeText(), QString("07:08:09"));

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
    QTRY_VERIFY(!popup->property("opened").toBool());

    controller->setLive(false);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QObject* nowButton = firstObjectWithObjectName(root, QStringLiteral("dateTimePopupNowButton"));
    QVERIFY(nowButton != nullptr);
    QVERIFY(activateControl(nowButton));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QTRY_VERIFY(controller->live());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlDateTimePopupTests)

#include "QmlDateTimePopupTests.moc"
