#include "SkyContextController.hpp"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    SkyContextController skyContextController;

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
