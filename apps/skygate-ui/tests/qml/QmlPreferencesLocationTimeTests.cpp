#include "QmlPreferencesTestSupport.hpp"

class QmlPreferencesLocationTimeTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void locationCoordinateChangesPropagateToSkyView();
    void timeZonePickerFiltersSelectsAndApplies();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesLocationTimeTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesLocationTimeTests")));
}

void QmlPreferencesLocationTimeTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesLocationTimeTests::locationCoordinateChangesPropagateToSkyView()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller, sceneModel.get());

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 420
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesSkySection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesLocationTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    renderSignature(*controller, *sceneModel);
    const std::uint64_t baselineSnapshotGeneration = sceneModel->snapshotGeneration();
    auto* latitudeInput = firstQuickItemWithObjectName(root, QStringLiteral("latitudeInput"));
    auto* longitudeInput = firstQuickItemWithObjectName(root, QStringLiteral("longitudeInput"));
    auto* elevationInput = firstQuickItemWithObjectName(root, QStringLiteral("elevationInput"));
    QVERIFY(latitudeInput != nullptr);
    QVERIFY(longitudeInput != nullptr);
    QVERIFY(elevationInput != nullptr);

    replaceText(
        exposed.window(),
        latitudeInput,
        QStringLiteral("12.5")
    );
    replaceText(
        exposed.window(),
        longitudeInput,
        QStringLiteral("34.5")
    );
    replaceText(
        exposed.window(),
        elevationInput,
        QStringLiteral("88")
    );
    QCOMPARE(draft->property("locationSourceText").toString(), QString("Custom"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));

    QCOMPARE(controller->locationSourceText(), QString("Custom"));
    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    QTRY_VERIFY(sceneModel->snapshotGeneration() > baselineSnapshotGeneration);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesLocationTimeTests::timeZonePickerFiltersSelectsAndApplies()
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
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesSkySection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesTimeZonePickerTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    auto* pickerMouse = qobject_cast<QQuickItem*>(
        firstObjectWithObjectName(root, QStringLiteral("timeZonePickerMouse"))
    );
    QVERIFY(pickerMouse != nullptr);
    const QPointF pickerCenter = pickerMouse->mapToScene(
        QPointF(pickerMouse->width() / 2.0, pickerMouse->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, pickerCenter.toPoint());

    auto* searchInput = firstQuickItemWithObjectName(
        root,
        QStringLiteral("timeZoneSearchField")
    );
    QVERIFY(searchInput != nullptr);
    replaceText(exposed.window(), searchInput, QStringLiteral("bishkek"));
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Asia/Bishkek")) != nullptr);

    auto* delegateMouse = qobject_cast<QQuickItem*>(
        firstObjectWithObjectName(root, QStringLiteral("timeZoneDelegateMouse_Asia/Bishkek"))
    );
    QVERIFY(delegateMouse != nullptr);
    const QPointF delegateCenter = delegateMouse->mapToScene(
        QPointF(delegateMouse->width() / 2.0, delegateMouse->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, delegateCenter.toPoint());

    QTRY_COMPARE(draft->property("timeZoneId").toString(), QString("Asia/Bishkek"));
    QVERIFY(draft->property("timeZoneDisplayText").toString().contains("UTC+06:00"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesLocationTimeTests)

#include "QmlPreferencesLocationTimeTests.moc"
