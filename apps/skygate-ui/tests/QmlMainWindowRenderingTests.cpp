#include "QmlRenderingTestSupport.hpp"

class QmlMainWindowRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds();

private:
    QmlSettingsFixture m_settings;
};

void QmlMainWindowRenderingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlMainWindowRenderingTests")));
}

void QmlMainWindowRenderingTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlMainWindowRenderingTests::mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds()
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

    for (const QSize size : {QSize(1100, 760), QSize(560, 640)}) {
        rootWindow->resize(size);
        QCoreApplication::processEvents();
        QTRY_VERIFY(windowHasMultipleSampledColors(*rootWindow));
        QString failure;
        bool itemsFit = false;
        for (int attempt = 0; attempt < 30 && !itemsFit; ++attempt) {
            failure.clear();
            itemsFit = visibleQuickItemsFitWithinWindow(*rootWindow, &failure);
            if (!itemsFit) {
                QTest::qWait(10);
                QCoreApplication::processEvents();
            }
        }
        QVERIFY2(itemsFit, qPrintable(failure));

        const QImage windowImage = rootWindow->grabWindow();
        QVERIFY(!windowImage.isNull());
        QObject* theme = controller->theme();
        QVERIFY(theme != nullptr);
        auto* footer = firstQuickItemWithObjectName(rootWindow, QStringLiteral("statusFooter"));
        QVERIFY(footer != nullptr);
        QCOMPARE(footer->height(), 48.0);
        QVERIFY2(
            renderingScenePointIsColorNear(
                *rootWindow,
                windowImage,
                footer->mapToScene(QPointF(4.0, footer->height() * 0.5)),
                theme->property("footerBackground").value<QColor>(),
                4
            ),
            "Status footer did not paint the expected background at a stable empty edge"
        );

        for (const QString& objectName : {
            QStringLiteral("searchToolbarToggle"),
            QStringLiteral("timelineToolbarToggle"),
        }) {
            auto* toggle = firstQuickItemWithObjectName(rootWindow, objectName);
            QVERIFY(toggle != nullptr);
            QVERIFY2(
                renderingItemRegionContainsColorNear(
                    *rootWindow,
                    windowImage,
                    *toggle,
                    theme->property("toolbarToggleBorder").value<QColor>(),
                    24
                ),
                qPrintable(QStringLiteral("%1 did not paint its toggle border").arg(objectName))
            );
        }

        auto* searchToggle = firstQuickItemWithObjectName(
            rootWindow,
            QStringLiteral("searchToolbarToggle")
        );
        auto* timelineToggle = firstQuickItemWithObjectName(
            rootWindow,
            QStringLiteral("timelineToolbarToggle")
        );
        auto* viewport = firstQuickItemWithObjectName(rootWindow, QStringLiteral("skyViewport"));
        QVERIFY(searchToggle != nullptr);
        QVERIFY(timelineToggle != nullptr);
        QVERIFY(viewport != nullptr);
        QVERIFY(renderingQuickItemSceneRect(*searchToggle).intersects(
            renderingQuickItemSceneRect(*viewport)
        ));
        QVERIFY(renderingQuickItemSceneRect(*timelineToggle).intersects(
            renderingQuickItemSceneRect(*viewport)
        ));
        QVERIFY(!renderingQuickItemSceneRect(*searchToggle).intersects(
            renderingQuickItemSceneRect(*timelineToggle)
        ));
    }

    QObject* aboutItem = firstObjectWithObjectName(rootWindow, QStringLiteral("aboutMenuItem"));
    QVERIFY(aboutItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(renderingTriggerMenuItem(aboutItem));
    auto* aboutWindow = qobject_cast<QQuickWindow*>(renderingObjectWithWindowTitle(
        rootWindow,
        QStringLiteral("About SkyGate")
    ));
    QVERIFY(aboutWindow != nullptr);
    QTRY_VERIFY(aboutWindow->isVisible());
    QTRY_VERIFY(windowHasMultipleSampledColors(*aboutWindow));
    QString failure;
    QVERIFY2(visibleQuickItemsFitWithinWindow(*aboutWindow, &failure), qPrintable(failure));

    QObject* preferencesItem = firstObjectWithObjectName(
        rootWindow,
        QStringLiteral("preferencesMenuItem")
    );
    QVERIFY(preferencesItem != nullptr);
    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(renderingTriggerMenuItem(preferencesItem));
    auto* preferencesWindow = qobject_cast<QQuickWindow*>(renderingObjectWithWindowTitle(
        rootWindow,
        QStringLiteral("Preferences")
    ));
    QVERIFY(preferencesWindow != nullptr);
    QTRY_VERIFY(preferencesWindow->isVisible());
    QTRY_VERIFY(windowHasMultipleSampledColors(*preferencesWindow));
    failure.clear();
    QVERIFY2(visibleQuickItemsFitWithinWindow(*preferencesWindow, &failure), qPrintable(failure));

    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlMainWindowRenderingTests)

#include "QmlMainWindowRenderingTests.moc"
