#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlMainWindowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void responsiveToolbarPolicyKeepsControlsSeparated();
    void topToolbarsStayAboveOverlay();
    void footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar();
    void nativeMenuActionsOpenSharedAboutAndPreferencesWindows();
    void compactAppMenuOpensAboutAndPreferencesWindows();
    void mainWindowPreferenceSearchAndTrackingJourney();

private:
    QmlSettingsFixture m_settings;
};

namespace {

QObject* windowWithObjectName(QObject* root, const QString& objectName)
{
    for (QObject* object : objectTree(root)) {
        if (qobject_cast<QWindow*>(object) != nullptr && object->objectName() == objectName) {
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

    controller->setSearchToolbarCollapsed(false);
    controller->setTimelineToolbarCollapsed(false);
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(rootWindow, "prepareTimelineToolbarExpand"));
    QVERIFY(!controller->searchToolbarCollapsed());

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

    auto* searchToolbar = firstQuickItemWithObjectName(rootWindow, QStringLiteral("searchToolbar"));
    auto* timelineToolbar = firstQuickItemWithObjectName(
        rootWindow,
        QStringLiteral("timelineToolbar")
    );
    auto* appMenuButton = firstQuickItemWithObjectName(rootWindow, QStringLiteral("appMenuButton"));
    auto* overlayLayer = firstQuickItemWithObjectName(rootWindow, QStringLiteral("skyOverlayLayer"));

    QVERIFY(searchToolbar != nullptr);
    QVERIFY(timelineToolbar != nullptr);
    QVERIFY(appMenuButton != nullptr);
    QVERIFY(overlayLayer != nullptr);
    QVERIFY(searchToolbar->z() > overlayLayer->z());
    QVERIFY(timelineToolbar->z() > overlayLayer->z());
    QVERIFY(appMenuButton->z() > overlayLayer->z());
    QCOMPARE(timelineToolbar->z(), searchToolbar->z());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    auto* timeMouse = firstQuickItemWithObjectName(rootWindow, QStringLiteral("statusTimeMouse"));
    auto* nightMouse = firstQuickItemWithObjectName(
        rootWindow,
        QStringLiteral("nightConditionsMouse")
    );
    auto* searchToggle = firstQuickItemWithObjectName(
        rootWindow,
        QStringLiteral("searchToolbarToggle")
    );
    auto* timelineToggle = firstQuickItemWithObjectName(
        rootWindow,
        QStringLiteral("timelineToolbarToggle")
    );
    QObject* datePopup = firstObjectWithObjectName(rootWindow, QStringLiteral("dateTimePopup"));
    QObject* nightPopup = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("nightConditionsPopup")
    );

    QVERIFY(timeMouse != nullptr);
    QVERIFY(nightMouse != nullptr);
    QVERIFY(searchToggle != nullptr);
    QVERIFY(timelineToggle != nullptr);
    QVERIFY(datePopup != nullptr);
    QVERIFY(nightPopup != nullptr);

    const auto clickCenter = [](QQuickItem* item) {
        return item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0)).toPoint();
    };

    QTest::mouseClick(rootWindow, Qt::LeftButton, Qt::NoModifier, clickCenter(timeMouse));
    QTRY_VERIFY(datePopup->property("opened").toBool());
    QVERIFY(!controller->searchToolbarCollapsed());

    QTest::mouseClick(rootWindow, Qt::LeftButton, Qt::NoModifier, clickCenter(searchToggle));
    QTRY_VERIFY(controller->searchToolbarCollapsed());
    QTRY_VERIFY(!datePopup->property("opened").toBool());

    QTest::mouseClick(rootWindow, Qt::LeftButton, Qt::NoModifier, clickCenter(nightMouse));
    QTRY_VERIFY(nightPopup->property("opened").toBool());
    QVERIFY(!controller->timelineToolbarCollapsed());

    QTest::mouseClick(rootWindow, Qt::LeftButton, Qt::NoModifier, clickCenter(timelineToggle));
    QTRY_VERIFY(controller->timelineToolbarCollapsed());
    QTRY_VERIFY(!nightPopup->property("opened").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlMainWindowTests::nativeMenuActionsOpenSharedAboutAndPreferencesWindows()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    QObject* nativeMenuBar = firstObjectWithObjectName(rootWindow, QStringLiteral("nativeMenuBar"));
    QVERIFY(nativeMenuBar != nullptr);
#ifdef Q_OS_MACOS
    QVERIFY(nativeMenuBar->property("visible").toBool());
#else
    QVERIFY(!nativeMenuBar->property("visible").toBool());
#endif

    QObject* aboutItem = firstObjectWithObjectName(rootWindow, QStringLiteral("aboutMenuItem"));
    QVERIFY(aboutItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(aboutItem));
    QObject* aboutWindow = windowWithObjectName(rootWindow, QStringLiteral("aboutWindow"));
    QVERIFY(aboutWindow != nullptr);
    QTRY_VERIFY(aboutWindow->property("visible").toBool());

    QObject* preferencesItem = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("preferencesMenuItem")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(preferencesItem));
    QObject* preferencesWindow = windowWithObjectName(
        rootWindow,
        QStringLiteral("preferencesWindow")
    );
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->property("visible").toBool());
    auto* preferencesQuickWindow = qobject_cast<QQuickWindow*>(preferencesWindow);
    QVERIFY(preferencesQuickWindow != nullptr);

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

void QmlMainWindowTests::compactAppMenuOpensAboutAndPreferencesWindows()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    const QmlWarningScope warnings;
    auto* rootWindow = loadMainWindow(engine, *controller, *sceneModel);
    QVERIFY(rootWindow != nullptr);

    auto* appMenuButton = firstQuickItemWithObjectName(rootWindow, QStringLiteral("appMenuButton"));
    QVERIFY(appMenuButton != nullptr);
    QVERIFY(appMenuButton->isVisible());

    QObject* appMenuPopup = firstObjectWithObjectName(rootWindow, QStringLiteral("appMenuPopup"));
    QVERIFY(appMenuPopup != nullptr);
    QVERIFY(!appMenuPopup->property("opened").toBool());

    QVERIFY(activateControl(appMenuButton));
    QTRY_VERIFY(appMenuPopup->property("opened").toBool());

    QVERIFY(activateControl(appMenuButton));
    QTRY_VERIFY(!appMenuPopup->property("opened").toBool());
    QTest::qWait(160);

    QVERIFY(activateControl(appMenuButton));
    QTRY_VERIFY(appMenuPopup->property("opened").toBool());

    QObject* preferencesItem = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("appMenuPreferencesItem")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(activateControl(preferencesItem));
    QTRY_VERIFY(!appMenuPopup->property("opened").toBool());
    QObject* preferencesWindow = windowWithObjectName(
        rootWindow,
        QStringLiteral("preferencesWindow")
    );
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->property("visible").toBool());

    QVERIFY(activateControl(appMenuButton));
    QTRY_VERIFY(appMenuPopup->property("opened").toBool());

    QObject* aboutItem = firstObjectWithObjectName(rootWindow, QStringLiteral("appMenuAboutItem"));
    QVERIFY(aboutItem != nullptr);
    QCOMPARE(aboutItem->property("text").toString(), QString("About"));
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(activateControl(aboutItem));
    QTRY_VERIFY(!appMenuPopup->property("opened").toBool());
    QObject* aboutWindow = windowWithObjectName(rootWindow, QStringLiteral("aboutWindow"));
    QVERIFY(aboutWindow != nullptr);
    QTRY_VERIFY(aboutWindow->property("visible").toBool());

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

    QObject* preferencesItem = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("preferencesMenuItem")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(triggerMenuItem(preferencesItem));
    QObject* preferencesWindow = windowWithObjectName(
        rootWindow,
        QStringLiteral("preferencesWindow")
    );
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->property("visible").toBool());
    auto* preferencesQuickWindow = qobject_cast<QQuickWindow*>(preferencesWindow);
    QVERIFY(preferencesQuickWindow != nullptr);

    auto* latitudeInput = firstQuickItemWithObjectName(
        preferencesWindow,
        QStringLiteral("latitudeInput")
    );
    auto* longitudeInput = firstQuickItemWithObjectName(
        preferencesWindow,
        QStringLiteral("longitudeInput")
    );
    QVERIFY(latitudeInput != nullptr);
    QVERIFY(longitudeInput != nullptr);
    replaceText(preferencesQuickWindow, latitudeInput, QStringLiteral("35.689500"));
    replaceText(preferencesQuickWindow, longitudeInput, QStringLiteral("139.691700"));

    QObject* appearanceButton = firstObjectWithObjectName(
        preferencesWindow,
        QStringLiteral("preferencesAppearanceSectionButton")
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

    QObject* applyButton = firstObjectWithObjectName(
        preferencesWindow,
        QStringLiteral("preferencesApplyButton")
    );
    QVERIFY(applyButton != nullptr);
    QVERIFY(activateControl(applyButton));
    QTRY_COMPARE(controller->latitudeText(), QString("35.689500"));
    QCOMPARE(controller->longitudeText(), QString("139.691700"));
    QCOMPARE(controller->themeId(), nextThemeId);
    QTRY_VERIFY(sceneModel->snapshotGeneration() > baselineGeneration);

    preferencesWindow->setProperty("visible", false);
    QTRY_VERIFY(!preferencesWindow->property("visible").toBool());
    rootWindow->requestActivate();
    QCoreApplication::processEvents();

    auto* searchInput = firstQuickItemWithObjectName(rootWindow, QStringLiteral("searchField"));
    QVERIFY(searchInput != nullptr);
    replaceText(rootWindow, searchInput, QStringLiteral("Sirius"));
    QTRY_VERIFY(controller->objectSearchModel()->rowCount() > 0);
    QTest::keyClick(rootWindow, Qt::Key_Return);
    QTRY_COMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("sirius"));
    QTRY_VERIFY(firstVisibleItemWithText(rootWindow, QStringLiteral("Sirius")) != nullptr);

    QObject* trackButton = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("objectInspectorTrackButton")
    );
    QVERIFY(trackButton != nullptr);
    QVERIFY(activateControl(trackButton));
    QTRY_VERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetId(), QString("sirius"));

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlMainWindowTests)

#include "QmlMainWindowTests.moc"
