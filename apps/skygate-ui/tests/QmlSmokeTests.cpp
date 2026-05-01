#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <QtTest/QtTest>

#include <QGuiApplication>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QUrl>
#include <qqml.h>

#include <memory>

#ifndef SKYGATE_QML_SOURCE_DIR
#define SKYGATE_QML_SOURCE_DIR ""
#endif

class QmlSmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void mainQmlLoadsWithRealContextObjects();
};

void QmlSmokeTests::mainQmlLoadsWithRealContextObjects()
{
    qmlRegisterType<SkyViewportItem>("com.skygate.app", 1, 0, "SkyViewportItem");

    auto starCatalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(starCatalog != nullptr);
    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    QVERIFY(ephemerisEngine != nullptr);

    SkyContextController skyContextController(std::move(starCatalog), std::move(ephemerisEngine));
    SkySceneModel skySceneModel;
    skySceneModel.setSkyContextController(&skyContextController);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("skyContext", &skyContextController);
    engine.rootContext()->setContextProperty("skyScene", &skySceneModel);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));

    bool objectCreationFailed = false;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &engine,
        [&objectCreationFailed] {
            objectCreationFailed = true;
        }
    );

    const QUrl mainQmlUrl = QUrl::fromLocalFile(
        QStringLiteral(SKYGATE_QML_SOURCE_DIR) + QStringLiteral("/Main.qml")
    );
    engine.load(mainQmlUrl);
    QCoreApplication::processEvents();

    QVERIFY(!objectCreationFailed);
    QVERIFY(!engine.rootObjects().isEmpty());
}

int main(int argc, char* argv[])
{
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QGuiApplication app(argc, argv);
    QFont::insertSubstitution(QStringLiteral("Sans Serif"), QStringLiteral("Avenir Next"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Menlo"));
    app.setFont(QFont(QStringLiteral("Avenir Next")));
    QmlSmokeTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "QmlSmokeTests.moc"
