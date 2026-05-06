#include "QmlPreferencesTestSupport.hpp"

class QmlPreferencesAppearanceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void generalLoggingControlsBindDraft();
    void appearanceThemeSwitchWorks();
    void appearanceCheckboxChangesRender_data();
    void appearanceCheckboxChangesRender();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesAppearanceTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesAppearanceTests")));
}

void QmlPreferencesAppearanceTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesAppearanceTests::generalLoggingControlsBindDraft()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLogToTerminal(true);
    controller->setLogToFile(false);

    QQmlEngine engine;
    setupEngine(engine, *controller);

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
            PreferencesGeneralSection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesGeneralLoggingTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* terminalBox = firstObjectWithObjectName(
        root,
        QStringLiteral("logToTerminalCheckBox")
    );
    QObject* fileBox = firstObjectWithObjectName(root, QStringLiteral("logToFileCheckBox"));
    QVERIFY(terminalBox != nullptr);
    QVERIFY(fileBox != nullptr);
    terminalBox->setProperty("checked", false);
    QVERIFY(QMetaObject::invokeMethod(terminalBox, "toggled"));
    fileBox->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(fileBox, "toggled"));
    QCOMPARE(draft->property("logToTerminal").toBool(), false);
    QCOMPARE(draft->property("logToFile").toBool(), true);

    auto* logFileInput = firstQuickItemWithObjectName(root, QStringLiteral("logFilePathField"));
    QVERIFY(logFileInput != nullptr);
    QVERIFY2(logFileInput->width() >= 260.0, "Log file field should stay wide enough to read paths");
    logFileInput->setProperty("text", QStringLiteral("/tmp/skygate-qml-preferences.log"));
    QVERIFY(QMetaObject::invokeMethod(logFileInput, "editingFinished"));
    QTRY_COMPARE(
        draft->property("logFilePath").toString(),
        QString("/tmp/skygate-qml-preferences.log")
    );

    QObject* browseButton = firstObjectWithObjectName(root, QStringLiteral("logFileBrowseButton"));
    QVERIFY(browseButton != nullptr);
    QVERIFY(browseButton->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->logToTerminal(), false);
    QCOMPARE(controller->logToFile(), true);
    QCOMPARE(controller->logFilePath(), QString("/tmp/skygate-qml-preferences.log"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesAppearanceTests::appearanceThemeSwitchWorks()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    const QVariantList themeOptions = controller->themeOptions();
    QVERIFY(themeOptions.size() >= 2);
    const QString nextThemeId = themeOptions.at(1).toMap().value("id").toString();
    QVERIFY(!nextThemeId.isEmpty());
    const QColor originalStarColor = controller->renderTheme().bodyStar;

    QQmlEngine engine;
    setupEngine(engine, *controller);

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
            PreferencesAppearanceSection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesAppearanceThemeTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* themeCombo = firstObjectWithObjectName(root, QStringLiteral("themeCombo"));
    QVERIFY(themeCombo != nullptr);
    QCOMPARE(themeCombo->property("count").toInt(), themeOptions.size());
    themeCombo->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(themeCombo, "activated", Q_ARG(int, 1)));
    QCOMPARE(draft->property("themeId").toString(), nextThemeId);

    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->themeId(), nextThemeId);
    QVERIFY(controller->renderTheme().bodyStar != originalStarColor);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesAppearanceTests::appearanceCheckboxChangesRender_data()
{
    QTest::addColumn<QString>("checkboxObjectName");
    QTest::addColumn<QString>("name");

    QTest::newRow("horizon")
        << QStringLiteral("overlayHorizonCheckBox") << QStringLiteral("Horizon");
    QTest::newRow("alt-az-grid")
        << QStringLiteral("overlayAltAzGridCheckBox") << QStringLiteral("Alt/Az grid");
    QTest::newRow("constellation-lines")
        << QStringLiteral("overlayConstellationLinesCheckBox") << QStringLiteral("Constellation lines");
    QTest::newRow("constellation-labels")
        << QStringLiteral("overlayConstellationLabelsCheckBox") << QStringLiteral("Constellation labels");
    QTest::newRow("solar-system-labels")
        << QStringLiteral("overlaySolarSystemLabelsCheckBox") << QStringLiteral("Solar system labels");
    QTest::newRow("deep-sky-objects")
        << QStringLiteral("overlayDeepSkyObjectsCheckBox") << QStringLiteral("Deep sky objects");
    QTest::newRow("deep-sky-labels")
        << QStringLiteral("overlayDeepSkyLabelsCheckBox") << QStringLiteral("Deep sky labels");
    QTest::newRow("ecliptic")
        << QStringLiteral("overlayEclipticCheckBox") << QStringLiteral("Ecliptic");
    QTest::newRow("celestial-equator")
        << QStringLiteral("overlayCelestialEquatorCheckBox") << QStringLiteral("Celestial equator");
    QTest::newRow("circumpolar-boundary")
        << QStringLiteral("overlayCircumpolarBoundaryCheckBox") << QStringLiteral("Circumpolar boundary");
}

void QmlPreferencesAppearanceTests::appearanceCheckboxChangesRender()
{
    QFETCH(QString, checkboxObjectName);
    QFETCH(QString, name);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setMagnitudeCutoff(8.0);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller, sceneModel.get());

    const QmlWarningScope warnings(QmlWarningScope::Forwarding::Disabled);
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
            PreferencesAppearanceSection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesAppearanceCheckboxTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* checkBox = firstObjectWithObjectName(root, checkboxObjectName);
    QVERIFY(checkBox != nullptr);
    const SkyViewRenderSignature baseline = renderSignature(*controller, *sceneModel);

    const bool previousChecked = checkBox->property("checked").toBool();
    checkBox->setProperty("checked", !previousChecked);
    QVERIFY(QMetaObject::invokeMethod(checkBox, "toggled"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    const SkyViewRenderSignature changed = renderSignature(*controller, *sceneModel);
    QVERIFY2(
        changed.differsFrom(baseline),
        qPrintable(QString("Changing %1 did not alter the sky-view render signature").arg(name))
    );
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesAppearanceTests)

#include "QmlPreferencesAppearanceTests.moc"
