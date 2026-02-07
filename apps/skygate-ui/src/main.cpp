#include "SkyContextController.hpp"
#include "SkyViewportItem.hpp"

#include <QGuiApplication>
#include <QFont>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <qqml.h>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <utility>

int main(int argc, char* argv[])
{
    // Prevent stale cached QML artifacts from surfacing outdated warnings at launch.
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);
    QFont::insertSubstitution(QStringLiteral("Sans Serif"), QStringLiteral("Avenir Next"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Menlo"));
    app.setFont(QFont(QStringLiteral("Avenir Next")));
    QCoreApplication::setOrganizationName("Skygate");
    QCoreApplication::setOrganizationDomain("skygate.app");
    QCoreApplication::setApplicationName("Skygate");

    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog =
        skygate::ephemeris::createBundledStarCatalog();
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine =
        skygate::ephemeris::createEphemerisEngineStub(starCatalog.get());
    SkyContextController skyContextController(std::move(starCatalog), std::move(ephemerisEngine));
    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &app,
        [&skyContextController] {
            skyContextController.saveSettings();
        }
    );

    qmlRegisterType<SkyViewportItem>("com.skygate.app", 1, 0, "SkyViewportItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("skyContext", &skyContextController);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("com.skygate.app", "Main");
    return app.exec();
}
