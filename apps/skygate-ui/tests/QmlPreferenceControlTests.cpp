#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

namespace {

QQuickItem* cityDelegate(QObject* root)
{
    for (QObject* object : objectTree(root)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item != nullptr
            && item->isVisible()
            && !object->property("cityId").toString().isEmpty()
            && object->property("selectable").toBool()
        ) {
            return item;
        }
    }
    return nullptr;
}

void clickItemCenter(QWindow* window, QQuickItem* item)
{
    QVERIFY(item != nullptr);
    const QPoint clickPoint = item->mapToScene(
        QPointF(item->width() * 0.5, item->height() * 0.5)
    ).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPoint);
}

}  // namespace

class QmlPreferenceControlTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void preferencesTextFieldEditsAcceptsAndTracksEnabledState();
    void preferencesComboBoxActivatesAndShowsPopup();
    void preferencesCityPickerFiltersChoosesAndClearsModelFilter();
    void preferencesButtonCheckboxAndSectionControlsRespond();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferenceControlTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferenceControlTests")));
}

void QmlPreferenceControlTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferenceControlTests::preferencesTextFieldEditsAcceptsAndTracksEnabledState()
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
            width: 320
            height: 80
            property int acceptedCount: 0
            property string lastText: ""
            PreferencesTextField {
                id: field
                anchors.fill: parent
                placeholderText: "Type here"
                onTextEdited: root.lastText = text
                onAccepted: ++root.acceptedCount
            }
            property alias field: field
        }
    )"), QStringLiteral("PreferencesTextFieldBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    auto* field = qobject_cast<QQuickItem*>(qvariant_cast<QObject*>(root->property("field")));
    QVERIFY(field != nullptr);

    QVERIFY(QMetaObject::invokeMethod(field, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("hello"));
    QTRY_COMPARE(field->property("text").toString(), QString("hello"));
    QCOMPARE(root->property("lastText").toString(), QString("hello"));
    QTest::keyClick(exposed.window(), Qt::Key_Return);
    QTRY_COMPARE(root->property("acceptedCount").toInt(), 1);

    field->setProperty("enabled", false);
    QCoreApplication::processEvents();
    QVERIFY(!field->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferenceControlTests::preferencesComboBoxActivatesAndShowsPopup()
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
            width: 260
            height: 80
            property int activatedIndex: -1
            PreferencesComboBox {
                id: combo
                width: 220
                model: ["One", "Two", "Three"]
                onActivated: function(index) { root.activatedIndex = index }
            }
            property alias combo: combo
        }
    )"), QStringLiteral("PreferencesComboBoxBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* combo = qvariant_cast<QObject*>(root->property("combo"));
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->property("displayText").toString(), QString("One"));

    QObject* popup = qvariant_cast<QObject*>(combo->property("popup"));
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    QTRY_VERIFY(!popup->property("opened").toBool());

    combo->setProperty("currentIndex", 2);
    QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, 2)));
    QCOMPARE(root->property("activatedIndex").toInt(), 2);
    QCOMPARE(combo->property("displayText").toString(), QString("Three"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferenceControlTests::preferencesCityPickerFiltersChoosesAndClearsModelFilter()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto* cityModel = controller->cityCatalogModel();
    QVERIFY(cityModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 360
            height: 420
            property string chosenCityId: ""
            property string chosenDisplayText: ""
            property real chosenLatitude: 0
            property real chosenLongitude: 0
            PreferencesCityPicker {
                id: picker
                width: 320
                height: 32
                catalogModel: skyContext.cityCatalogModel
                selectedText: ""
                onCityChosen: function(cityId, displayText, latitudeDeg, longitudeDeg) {
                    root.chosenCityId = cityId
                    root.chosenDisplayText = displayText
                    root.chosenLatitude = latitudeDeg
                    root.chosenLongitude = longitudeDeg
                }
            }
            property alias picker: picker
        }
    )"), QStringLiteral("PreferencesCityPickerBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    auto* picker = qobject_cast<QQuickItem*>(qvariant_cast<QObject*>(root->property("picker")));
    QVERIFY(picker != nullptr);
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, QPoint(16, 16));

    auto* filterField = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        root,
        "placeholderText",
        QStringLiteral("Filter by city or country")
    ));
    QVERIFY(filterField != nullptr);
    QTRY_VERIFY(filterField->hasActiveFocus());
    replaceText(exposed.window(), filterField, QStringLiteral("Zurich"));
    QTRY_COMPARE(cityModel->property("filterText").toString(), QString("zurich"));

    auto* zurichDelegate = cityDelegate(root);
    QVERIFY(zurichDelegate != nullptr);
    const QPoint clickPoint = zurichDelegate->mapToScene(
        QPointF(zurichDelegate->width() * 0.5, zurichDelegate->height() * 0.5)
    ).toPoint();
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, clickPoint);
    QTRY_VERIFY(!root->property("chosenCityId").toString().isEmpty());
    QVERIFY(root->property("chosenDisplayText").toString().contains(QStringLiteral("Zurich")));
    QVERIFY(root->property("chosenLatitude").toDouble() > 40.0);
    QVERIFY(root->property("chosenLongitude").toDouble() > 0.0);
    QTRY_COMPARE(cityModel->property("filterText").toString(), QString());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferenceControlTests::preferencesButtonCheckboxAndSectionControlsRespond()
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
            width: 360
            height: 160
            property int actionClicks: 0
            property int sectionClicks: 0
            PreferencesActionButton {
                id: actionButton
                x: 10
                y: 10
                text: "Apply"
                primary: true
                onClicked: ++root.actionClicks
            }
            PreferencesCheckBox {
                id: checkBox
                x: 10
                y: 56
            }
            PreferencesSectionButton {
                id: sectionButton
                x: 10
                y: 94
                width: 180
                label: "Appearance"
                active: false
                onClicked: {
                    active = true
                    ++root.sectionClicks
                }
            }
            property alias actionButton: actionButton
            property alias checkBox: checkBox
            property alias sectionButton: sectionButton
        }
    )"), QStringLiteral("PreferencesSmallControlBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    auto* actionButton = qobject_cast<QQuickItem*>(
        qvariant_cast<QObject*>(root->property("actionButton"))
    );
    auto* checkBox = qobject_cast<QQuickItem*>(
        qvariant_cast<QObject*>(root->property("checkBox"))
    );
    auto* sectionButton = qobject_cast<QQuickItem*>(
        qvariant_cast<QObject*>(root->property("sectionButton"))
    );
    QVERIFY(actionButton != nullptr);
    QVERIFY(checkBox != nullptr);
    QVERIFY(sectionButton != nullptr);

    clickItemCenter(exposed.window(), actionButton);
    QTRY_COMPARE(root->property("actionClicks").toInt(), 1);

    actionButton->setProperty("enabled", false);
    QCoreApplication::processEvents();
    clickItemCenter(exposed.window(), actionButton);
    QCOMPARE(root->property("actionClicks").toInt(), 1);

    QVERIFY(!checkBox->property("checked").toBool());
    clickItemCenter(exposed.window(), checkBox);
    QTRY_VERIFY(checkBox->property("checked").toBool());
    clickItemCenter(exposed.window(), checkBox);
    QTRY_VERIFY(!checkBox->property("checked").toBool());

    QVERIFY(!sectionButton->property("active").toBool());
    clickItemCenter(exposed.window(), sectionButton);
    QTRY_COMPARE(root->property("sectionClicks").toInt(), 1);
    QVERIFY(sectionButton->property("active").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferenceControlTests)

#include "QmlPreferenceControlTests.moc"
