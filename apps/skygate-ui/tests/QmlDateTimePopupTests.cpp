#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlDateTimePopupTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void dateTimePopupValidatesAppliesAndCancels();

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
    popup->setProperty("stagedDateText", QStringLiteral("bad-date"));
    popup->setProperty("stagedTimeText", QStringLiteral("12:00:00"));
    QVERIFY(QMetaObject::invokeMethod(popup, "applyChanges"));
    QVERIFY(!popup->property("errorText").toString().isEmpty());
    QVERIFY(popup->property("opened").toBool());

    popup->setProperty("stagedDateText", QStringLiteral("2024-01-02"));
    popup->setProperty("stagedTimeText", QStringLiteral("03:04:05"));
    QVERIFY(QMetaObject::invokeMethod(popup, "applyChanges"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(controller->utcDateText(), QString("2024-01-02"));
    QCOMPARE(controller->utcTimeText(), QString("03:04:05"));

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "cancel"));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlDateTimePopupTests)

#include "QmlDateTimePopupTests.moc"
