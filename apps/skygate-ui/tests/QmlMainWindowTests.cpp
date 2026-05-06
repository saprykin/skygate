#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlMainWindowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void responsiveToolbarPolicyKeepsControlsSeparated();
    void topToolbarsStayAboveOverlay();
    void menuActionsOpenAboutAndPreferencesWindows();
    void mainWindowPreferenceSearchAndTrackingJourney();

private:
    QmlSettingsFixture m_settings;
};

namespace {

QObject* objectWithWindowTitle(QObject* root, const QString& title)
{
    for (QObject* object : objectTree(root)) {
        if (qobject_cast<QWindow*>(object) != nullptr && object->property("title") == title) {
            return object;
        }
    }
    return nullptr;
}

bool triggerMenuItem(QObject* menuItem)
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

QObject* firstObjectWithLabel(QObject* root, const QString& label)
{
    for (QObject* object : objectTree(root)) {
        if (object->property("label").toString() == label) {
            return object;
        }
    }
    return nullptr;
}

QQuickWindow* loadMainWindow(
    QQmlApplicationEngine& engine,
    SkyContextController& controller,
    SkySceneModel& sceneModel
)
{
    setupEngine(engine, controller, &sceneModel);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));
    engine.load(QUrl::fromLocalFile(qmlSourcePath(QStringLiteral("Main.qml"))));
    if (engine.rootObjects().isEmpty()) {
        return nullptr;
    }

    auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    if (rootWindow != nullptr) {
        (void)QTest::qWaitForWindowExposed(rootWindow);
    }
    return rootWindow;
}

}  // namespace

void QmlMainWindowTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlMainWindowTests")));
}

void QmlMainWindowTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlMainWindowTests::responsiveToolbarPolicyKeepsControlsSeparated()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    QVariant overlap;
    rootWindow->setWidth(1500);
    rootWindow->setHeight(760);
    controller->setSearchToolbarCollapsed(false);
    controller->setTimelineToolbarCollapsed(false);
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(
        rootWindow,
        "expandedToolbarsWouldOverlap",
        Q_RETURN_ARG(QVariant, overlap)
    ));
    QVERIFY(!overlap.toBool());
    QVERIFY(QMetaObject::invokeMethod(rootWindow, "prepareSearchToolbarExpand"));
    QVERIFY(!controller->timelineToolbarCollapsed());

    rootWindow->setWidth(520);
    rootWindow->setHeight(620);
    controller->setSearchToolbarCollapsed(false);
    controller->setTimelineToolbarCollapsed(false);
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(
        rootWindow,
        "expandedToolbarsWouldOverlap",
        Q_RETURN_ARG(QVariant, overlap)
    ));
    QVERIFY(overlap.toBool());
    QVERIFY(QMetaObject::invokeMethod(rootWindow, "prepareSearchToolbarExpand"));
    QTRY_VERIFY(controller->timelineToolbarCollapsed());

    controller->setSearchToolbarCollapsed(false);
    controller->setTimelineToolbarCollapsed(false);
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(rootWindow, "prepareTimelineToolbarExpand"));
    QTRY_VERIFY(controller->searchToolbarCollapsed());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlMainWindowTests::topToolbarsStayAboveOverlay()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    auto* searchToolbar = qobject_cast<QQuickItem*>(firstObjectWithMetaProperties(
        rootWindow,
        {"panelItem", "toggleItem", "dropdownItem"}
    ));
    auto* timelineToolbar = qobject_cast<QQuickItem*>(firstObjectWithMetaProperties(
        rootWindow,
        {"panelItem", "toggleItem", "speedValues"}
    ));
    auto* overlayLayer = qobject_cast<QQuickItem*>(firstObjectWithMetaProperties(
        rootWindow,
        {"sceneModel", "interactionLayer", "avoidItems"}
    ));

    QVERIFY(searchToolbar != nullptr);
    QVERIFY(timelineToolbar != nullptr);
    QVERIFY(overlayLayer != nullptr);
    QVERIFY(searchToolbar->z() > overlayLayer->z());
    QVERIFY(timelineToolbar->z() > overlayLayer->z());
    QCOMPARE(timelineToolbar->z(), searchToolbar->z());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlMainWindowTests::menuActionsOpenAboutAndPreferencesWindows()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    QObject* aboutItem = firstObjectWithProperty(
        rootWindow,
        "text",
        QStringLiteral("&About SkyGate")
    );
    QVERIFY(aboutItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(aboutItem));
    QObject* aboutWindow = objectWithWindowTitle(rootWindow, QStringLiteral("About SkyGate"));
    QVERIFY(aboutWindow != nullptr);
    QTRY_VERIFY(aboutWindow->property("visible").toBool());

    QObject* preferencesItem = firstObjectWithProperty(
        rootWindow,
        "text",
        QStringLiteral("&Preferences...")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(preferencesItem));
    QObject* preferencesWindow = objectWithWindowTitle(rootWindow, QStringLiteral("Preferences"));
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->property("visible").toBool());
    auto* preferencesQuickWindow = qobject_cast<QQuickWindow*>(preferencesWindow);
    QVERIFY(preferencesQuickWindow != nullptr);

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

void QmlMainWindowTests::mainWindowPreferenceSearchAndTrackingJourney()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText(
        QStringLiteral("2024-06-01"),
        QStringLiteral("22:00:00")
    ));
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);
    QTRY_VERIFY(sceneModel->snapshotGeneration() > 0U);
    const std::uint64_t baselineGeneration = sceneModel->snapshotGeneration();

    QObject* preferencesItem = firstObjectWithProperty(
        rootWindow,
        "text",
        QStringLiteral("&Preferences...")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(preferencesItem));
    QObject* preferencesWindow = objectWithWindowTitle(rootWindow, QStringLiteral("Preferences"));
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->property("visible").toBool());
    auto* preferencesQuickWindow = qobject_cast<QQuickWindow*>(preferencesWindow);
    QVERIFY(preferencesQuickWindow != nullptr);

    auto* latitudeInput = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        preferencesWindow,
        "placeholderText",
        QStringLiteral("-90..90")
    ));
    auto* longitudeInput = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        preferencesWindow,
        "placeholderText",
        QStringLiteral("-180..180")
    ));
    QVERIFY(latitudeInput != nullptr);
    QVERIFY(longitudeInput != nullptr);
    replaceText(preferencesQuickWindow, latitudeInput, QStringLiteral("35.689500"));
    replaceText(preferencesQuickWindow, longitudeInput, QStringLiteral("139.691700"));

    QObject* appearanceButton = firstObjectWithLabel(
        preferencesWindow,
        QStringLiteral("Appearance")
    );
    QVERIFY(appearanceButton != nullptr);
    QVERIFY(activateControl(appearanceButton));
    QTRY_COMPARE(preferencesWindow->property("selectedPage").toInt(), 2);

    const QVariantList themeOptions = controller->themeOptions();
    QVERIFY(themeOptions.size() >= 2);
    const QString nextThemeId = themeOptions.at(1).toMap().value("id").toString();
    QObject* themeCombo = firstObjectWithObjectName(preferencesWindow, QStringLiteral("themeCombo"));
    QVERIFY(themeCombo != nullptr);
    QCOMPARE(themeCombo->property("count").toInt(), themeOptions.size());
    themeCombo->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(themeCombo, "activated", Q_ARG(int, 1)));

    const auto applyButtons = invokableButtonsWithText(preferencesWindow, QStringLiteral("Apply"));
    QCOMPARE(applyButtons.size(), 1);
    QVERIFY(activateControl(applyButtons.front()));
    QTRY_COMPARE(controller->latitudeText(), QString("35.689500"));
    QCOMPARE(controller->longitudeText(), QString("139.691700"));
    QCOMPARE(controller->themeId(), nextThemeId);
    QTRY_VERIFY(sceneModel->snapshotGeneration() > baselineGeneration);

    preferencesWindow->setProperty("visible", false);
    QTRY_VERIFY(!preferencesWindow->property("visible").toBool());
    rootWindow->requestActivate();
    QCoreApplication::processEvents();

    auto* searchInput = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        rootWindow,
        "placeholderText",
        QStringLiteral("Search planets, stars, HIP, constellations")
    ));
    QVERIFY(searchInput != nullptr);
    replaceText(rootWindow, searchInput, QStringLiteral("Sirius"));
    QTRY_VERIFY(controller->objectSearchModel()->rowCount() > 0);
    QTest::keyClick(rootWindow, Qt::Key_Return);
    QTRY_COMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("sirius"));
    QTRY_VERIFY(firstVisibleItemWithText(rootWindow, QStringLiteral("Sirius")) != nullptr);

    const auto trackButtons = invokableButtonsWithText(rootWindow, QStringLiteral("Track"));
    QCOMPARE(trackButtons.size(), 1);
    QVERIFY(activateControl(trackButtons.front()));
    QTRY_VERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetId(), QString("sirius"));

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlMainWindowTests)

#include "QmlMainWindowTests.moc"
