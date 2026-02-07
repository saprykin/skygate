#include "SkyContextController.hpp"
#include "SkyViewportItem.hpp"
#include "MacDockIcon.hpp"

#include <QFont>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QSize>
#include <QWindow>
#include <qqml.h>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <utility>

#ifndef SKYGATE_APP_VERSION
#define SKYGATE_APP_VERSION "0.0.0"
#endif

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
    QCoreApplication::setApplicationName("SkyGate");
    QCoreApplication::setApplicationVersion(QStringLiteral(SKYGATE_APP_VERSION));
    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/icons/app-icon-16.png"), QSize(16, 16));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-32.png"), QSize(32, 32));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-64.png"), QSize(64, 64));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-128.png"), QSize(128, 128));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-256.png"), QSize(256, 256));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-512.png"), QSize(512, 512));
    appIcon.addFile(QStringLiteral(":/icons/app-icon-1024.png"), QSize(1024, 1024));
#if defined(Q_OS_MACOS)
    const QString bundleIconPath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/../Resources/Skygate.icns");
    if (QFileInfo::exists(bundleIconPath)) {
        appIcon.addFile(bundleIconPath);
    }
#endif
    QGuiApplication::setWindowIcon(appIcon);
#if defined(Q_OS_MACOS)
    if (QFileInfo::exists(bundleIconPath)) {
        skygate::ui::setMacDockIcon(bundleIconPath);
    }
#endif

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
    for (QObject* rootObject : engine.rootObjects()) {
        if (QWindow* rootWindow = qobject_cast<QWindow*>(rootObject)) {
            rootWindow->setIcon(appIcon);
        }
    }
    return app.exec();
}
