#include "QmlTestSupport.hpp"
#include "SkyTimeController.hpp"

using namespace skygate::ui::tests;

class QmlNightConditionsPopupTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void nightConditionsPopupOpensClosesAndRendersRows();
    void footerNightAndDateTimePopupsAreMutuallyExclusive();

private:
    QmlSettingsFixture m_settings;
};

void QmlNightConditionsPopupTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlNightConditionsPopupTests")));
}

void QmlNightConditionsPopupTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlNightConditionsPopupTests::nightConditionsPopupOpensClosesAndRendersRows()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    QVERIFY(controller->setUtcDateTimeText("2024-03-21", "12:00:00"));
    controller->setLatitudeText("47.3769");
    controller->setLongitudeText("8.5417");
    controller->setElevationText("408.0");

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 920
            height: 360
            StatusNightConditionsPopup {
                id: nightPopup
                anchors.fill: parent
                skyContextController: skyContext
                popupRightMargin: 8
                popupBottomMargin: 8
            }
            property alias popup: nightPopup
        }
    )"), QStringLiteral("NightConditionsPopupBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QObject* popup = qvariant_cast<QObject*>(root->property("popup"));
    QVERIFY(popup != nullptr);

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_VERIFY(
        firstQuickItemWithObjectName(root, QStringLiteral("nightConditionsPopupTitle")) != nullptr
    );
    QTRY_VERIFY(
        firstQuickItemWithObjectName(root, QStringLiteral("nightConditionsSunHeader")) != nullptr
    );
    QTRY_VERIFY(
        firstQuickItemWithObjectName(root, QStringLiteral("nightConditionsMoonHeader")) != nullptr
    );
    auto* moonRiseRow = firstQuickItemWithObjectName(
        root,
        QStringLiteral("nightConditionsMoonRiseRow")
    );
    auto* moonSetRow = firstQuickItemWithObjectName(
        root,
        QStringLiteral("nightConditionsMoonSetRow")
    );
    QVERIFY(moonRiseRow != nullptr);
    QVERIFY(moonSetRow != nullptr);
    QVERIFY(moonSetRow->y() > moonRiseRow->y());
    QTRY_VERIFY(
        firstQuickItemWithObjectName(
            root,
            QStringLiteral("nightConditionsSunRowLabel_Sunset")
        ) != nullptr
    );
    auto* locationLabel = firstQuickItemWithObjectName(
        root,
        QStringLiteral("nightConditionsLocationLabel")
    );
    QVERIFY(locationLabel != nullptr);
    QTRY_VERIFY(
        locationLabel->property("text").toString().contains(QStringLiteral("UTC+06:00"))
    );
    QVERIFY(controller->nightConditions().value("locationText").toString().contains("UTC+06:00"));
    QVERIFY(controller->nightConditions().value("valid").toBool());

    QTest::keyClick(exposed.window(), Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
    QTRY_VERIFY(!popup->property("opened").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlNightConditionsPopupTests::footerNightAndDateTimePopupsAreMutuallyExclusive()
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
            height: 360
            property alias datePopup: datePopup
            property alias nightPopup: nightPopup
            Item {
                id: contentArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: footer.top
            }
            QtObject {
                id: scene
                property var selectedObjectInspector: ({})
                function clearSelectedObjectInspector() {
                    selectedObjectInspector = ({})
                }
            }
            StatusFooter {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 48
                skyContextController: skyContext
                sceneModel: scene
                dateTimePopupOpen: datePopup.opened
                nightConditionsPopupOpen: nightPopup.opened
                onDateTimeClicked: {
                    nightPopup.close()
                    if (datePopup.opened) {
                        datePopup.cancel()
                        return
                    }
                    datePopup.open()
                }
                onNightConditionsClicked: {
                    datePopup.close()
                    if (nightPopup.opened) {
                        nightPopup.cancel()
                        return
                    }
                    nightPopup.open()
                }
            }
            StatusDateTimePopup {
                id: datePopup
                parent: contentArea
                anchors.fill: parent
                skyContextController: skyContext
                popupRightMargin: 8
                popupBottomMargin: 8
            }
            StatusNightConditionsPopup {
                id: nightPopup
                parent: contentArea
                anchors.fill: parent
                skyContextController: skyContext
                popupRightMargin: 96
                popupBottomMargin: 8
            }
        }
    )"), QStringLiteral("NightConditionsMainInteractionTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QQuickItem* night = firstQuickItemWithObjectName(root, QStringLiteral("nightConditionsMouse"));
    QQuickItem* time = firstQuickItemWithObjectName(root, QStringLiteral("statusTimeMouse"));
    QVERIFY(night != nullptr);
    QVERIFY(time != nullptr);

    QPointF nightCenter = night->mapToScene(QPointF(night->width() / 2.0, night->height() / 2.0));
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, nightCenter.toPoint());
    QObject* datePopup = qvariant_cast<QObject*>(root->property("datePopup"));
    QObject* nightPopup = qvariant_cast<QObject*>(root->property("nightPopup"));
    QVERIFY(datePopup != nullptr);
    QVERIFY(nightPopup != nullptr);
    QTRY_VERIFY(nightPopup->property("opened").toBool());
    QVERIFY(!datePopup->property("opened").toBool());

    QPointF timeCenter = time->mapToScene(QPointF(time->width() / 2.0, time->height() / 2.0));
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, timeCenter.toPoint());
    QTRY_VERIFY(datePopup->property("opened").toBool());
    QVERIFY(!nightPopup->property("opened").toBool());

    nightCenter = night->mapToScene(QPointF(night->width() / 2.0, night->height() / 2.0));
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, nightCenter.toPoint());
    QTRY_VERIFY(nightPopup->property("opened").toBool());
    QVERIFY(!datePopup->property("opened").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlNightConditionsPopupTests)

#include "QmlNightConditionsPopupTests.moc"
