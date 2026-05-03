#include "SkyContextController.hpp"
#include "SkyLogging.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"
#include "MacDockIcon.hpp"

#include <QFont>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QSize>
#include <QWindow>
#include <qqml.h>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <utility>

#ifndef SKYGATE_APP_VERSION
#define SKYGATE_APP_VERSION "0.0.0"
#endif

#ifndef SKYGATE_GIT_HASH
#define SKYGATE_GIT_HASH "unknown"
#endif

#ifndef SKYGATE_BUILD_DATE_TIME
#define SKYGATE_BUILD_DATE_TIME __DATE__ " " __TIME__
#endif

namespace {

void applyOutputText(skygate::ui::SkyLoggingConfiguration& configuration, const QString& outputText)
{
    const QString normalizedOutput = outputText.trimmed().toLower();
    if (normalizedOutput == QStringLiteral("terminal")) {
        configuration.logToTerminal = true;
        configuration.logToFile = false;
    } else if (normalizedOutput == QStringLiteral("file")) {
        configuration.logToTerminal = false;
        configuration.logToFile = true;
    } else if (normalizedOutput == QStringLiteral("both")) {
        configuration.logToTerminal = true;
        configuration.logToFile = true;
    } else if (normalizedOutput == QStringLiteral("none")) {
        configuration.logToTerminal = false;
        configuration.logToFile = false;
    }
}

skygate::ui::SkyLoggingConfiguration startupLoggingConfiguration(const QStringList& arguments)
{
    skygate::ui::SkyLoggingConfiguration configuration =
        skygate::ui::SkyLogging::defaultConfiguration();

    QSettings settings;
    configuration.logToTerminal = settings.value(
        QStringLiteral("skyContext/logging/logToTerminal"),
        configuration.logToTerminal
    ).toBool();
    configuration.logToFile = settings.value(
        QStringLiteral("skyContext/logging/logToFile"),
        configuration.logToFile
    ).toBool();
    configuration.logFilePath = settings.value(
        QStringLiteral("skyContext/logging/logFilePath"),
        configuration.logFilePath
    ).toString();

    applyOutputText(configuration, qEnvironmentVariable("SKYGATE_LOG_OUTPUT"));
    const QString envLogFilePath = qEnvironmentVariable("SKYGATE_LOG_FILE");
    if (!envLogFilePath.trimmed().isEmpty()) {
        configuration.logFilePath = envLogFilePath.trimmed();
        configuration.logToFile = true;
    }

    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--log-to-terminal")) {
            configuration.logToTerminal = true;
        } else if (argument == QStringLiteral("--no-log-to-terminal")) {
            configuration.logToTerminal = false;
        } else if (argument == QStringLiteral("--log-to-file")) {
            configuration.logToFile = true;
        } else if (argument == QStringLiteral("--log-file") && index + 1 < arguments.size()) {
            configuration.logFilePath = arguments.at(++index).trimmed();
            configuration.logToFile = true;
        }
    }

    return configuration;
}

}  // namespace

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
    const skygate::ui::SkyLoggingConfiguration loggingConfiguration =
        startupLoggingConfiguration(app.arguments());
    skygate::ui::SkyLogging::install(loggingConfiguration);

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
        skygate::ephemeris::createEphemerisEngine(*starCatalog);
    SkyContextController skyContextController(std::move(starCatalog), std::move(ephemerisEngine));
    skyContextController.setLogToTerminal(loggingConfiguration.logToTerminal);
    skyContextController.setLogToFile(loggingConfiguration.logToFile);
    skyContextController.setLogFilePath(loggingConfiguration.logFilePath);
    SkySceneModel skySceneModel;
    skySceneModel.setSkyContextController(&skyContextController);
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
    engine.rootContext()->setContextProperty("skyScene", &skySceneModel);
    engine.rootContext()->setContextProperty(
        "skygateBuildDateTime",
        QStringLiteral(SKYGATE_BUILD_DATE_TIME)
    );
    engine.rootContext()->setContextProperty("skygateGitHash", QStringLiteral(SKYGATE_GIT_HASH));
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
