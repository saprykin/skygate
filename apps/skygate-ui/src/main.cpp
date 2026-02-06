#include "SkyContextController.hpp"
#include "SkyViewportItem.hpp"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <qqml.h>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <utility>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Skygate");
    QCoreApplication::setOrganizationDomain("skygate.app");
    QCoreApplication::setApplicationName("Skygate");

    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog =
        skygate::ephemeris::createBundledStarCatalog();
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine =
        skygate::ephemeris::createEphemerisEngineStub(starCatalog.get());
    SkyContextController skyContextController(std::move(starCatalog), std::move(ephemerisEngine));

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
