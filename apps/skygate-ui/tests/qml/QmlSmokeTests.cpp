#include "app/SkyContextController.hpp"
#include "catalog/CatalogFactory.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "render/SkyViewportItem.hpp"
#include "scene/SkySceneModel.hpp"
#include "support/QmlObjectTreeTestSupport.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QtTest/QtTest>
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

    auto starCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(starCatalog != nullptr);
    auto ephemerisEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(*starCatalog);
    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(ephemerisEngineResult.status));
    auto ephemerisEngine = std::move(ephemerisEngineResult.engine);
    QVERIFY(ephemerisEngine != nullptr);

    SkyContextController skyContextController(std::move(starCatalog), std::move(ephemerisEngine));
    SkySceneModel skySceneModel;
    skySceneModel.setSkyContextController(&skyContextController);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(SKYGATE_QML_SOURCE_DIR));
    engine.rootContext()->setContextProperty("skyContext", &skyContextController);
    engine.rootContext()->setContextProperty("skyScene", &skySceneModel);
    engine.rootContext()->setContextProperty("skygateBuildDateTime", QString("test"));
    engine.rootContext()->setContextProperty("skygateGitHash", QString("test"));

    bool objectCreationFailed = false;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &engine, [&objectCreationFailed] {
        objectCreationFailed = true;
    });

    const QUrl mainQmlUrl = QUrl::fromLocalFile(QStringLiteral(SKYGATE_QML_SOURCE_DIR) + QStringLiteral("/Main.qml"));
    engine.load(mainQmlUrl);
    QCoreApplication::processEvents();

    // The smoke test proves that Main.qml instantiates against the real context
    // objects and that the sky interaction layer is wired into it. The other
    // object-name spot checks live in the focused QML suites (status footer,
    // date/time popup, night conditions).
    QVERIFY(!objectCreationFailed);
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* rootObject = engine.rootObjects().front();
    QVERIFY(
        skygate::ui::tests::firstObjectWithObjectName(rootObject, QStringLiteral("skyInteractionLayer")) != nullptr
    );
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
