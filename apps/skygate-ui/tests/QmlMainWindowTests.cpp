#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlMainWindowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void responsiveToolbarPolicyKeepsControlsSeparated();
    void menuActionsOpenAboutAndPreferencesWindows();

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
    setupEngine(engine, *controller, sceneModel.get());
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));

    const QmlWarningScope warnings;
    engine.load(QUrl::fromLocalFile(qmlSourcePath(QStringLiteral("Main.qml"))));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    QVERIFY(rootWindow != nullptr);
    (void)QTest::qWaitForWindowExposed(rootWindow);

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

void QmlMainWindowTests::menuActionsOpenAboutAndPreferencesWindows()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlApplicationEngine engine;
    setupEngine(engine, *controller, sceneModel.get());
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));

    const QmlWarningScope warnings;
    engine.load(QUrl::fromLocalFile(qmlSourcePath(QStringLiteral("Main.qml"))));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
    QVERIFY(rootWindow != nullptr);
    (void)QTest::qWaitForWindowExposed(rootWindow);

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

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlMainWindowTests)

#include "QmlMainWindowTests.moc"
