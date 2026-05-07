#include "SkyContextController.hpp"
#include "SkyLogging.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"
#include "MacDockIcon.hpp"

#include <QFont>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QSize>
#include <QSysInfo>
#include <QWindow>
#include <qqml.h>

#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <cstdio>
#include <cstring>
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

Q_LOGGING_CATEGORY(skygateAppLog, "skygate.app")

bool hasArgument(const int argc, char* argv[], const char* expectedArgument) noexcept
{
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], expectedArgument) == 0) {
            return true;
        }
    }

    return false;
}

struct StartupLoggingConfiguration final {
    skygate::ui::SkyLoggingConfiguration configuration;
    bool overrideApplied = false;
    QStringList ignoredLogLevelTexts;
};

bool applyOutputText(
    skygate::ui::SkyLoggingConfiguration& configuration,
    const QString& outputText
)
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
    } else {
        return false;
    }
    return true;
}

bool applyLevelText(
    skygate::ui::SkyLoggingConfiguration& configuration,
    const QString& levelText
)
{
    const auto minimumType = skygate::ui::SkyLogging::messageTypeFromLevelText(levelText);
    if (!minimumType.has_value()) {
        return false;
    }

    configuration.terminalMinimumType = *minimumType;
    configuration.fileMinimumType = *minimumType;
    return true;
}

StartupLoggingConfiguration startupLoggingConfiguration(const QStringList& arguments)
{
    StartupLoggingConfiguration startupConfiguration {
        .configuration = skygate::ui::SkyLogging::defaultConfiguration()
    };

    QSettings settings;
    startupConfiguration.configuration.logToTerminal = settings.value(
        QStringLiteral("skyContext/logging/logToTerminal"),
        startupConfiguration.configuration.logToTerminal
    ).toBool();
    startupConfiguration.configuration.logToFile = settings.value(
        QStringLiteral("skyContext/logging/logToFile"),
        startupConfiguration.configuration.logToFile
    ).toBool();
    startupConfiguration.configuration.logFilePath = settings.value(
        QStringLiteral("skyContext/logging/logFilePath"),
        startupConfiguration.configuration.logFilePath
    ).toString();

    startupConfiguration.overrideApplied = applyOutputText(
        startupConfiguration.configuration,
        qEnvironmentVariable("SKYGATE_LOG_OUTPUT")
    );
    const QString envLogFilePath = qEnvironmentVariable("SKYGATE_LOG_FILE");
    if (!envLogFilePath.trimmed().isEmpty()) {
        startupConfiguration.configuration.logFilePath = envLogFilePath.trimmed();
        startupConfiguration.configuration.logToFile = true;
        startupConfiguration.overrideApplied = true;
    }

    const QString envLogLevel = qEnvironmentVariable("SKYGATE_LOG_LEVEL");
    if (!envLogLevel.trimmed().isEmpty()) {
        if (applyLevelText(startupConfiguration.configuration, envLogLevel)) {
            startupConfiguration.overrideApplied = true;
        } else {
            startupConfiguration.ignoredLogLevelTexts.push_back(envLogLevel.trimmed());
        }
    }

    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--log-to-terminal")) {
            startupConfiguration.configuration.logToTerminal = true;
            startupConfiguration.overrideApplied = true;
        } else if (argument == QStringLiteral("--no-log-to-terminal")) {
            startupConfiguration.configuration.logToTerminal = false;
            startupConfiguration.overrideApplied = true;
        } else if (argument == QStringLiteral("--log-to-file")) {
            startupConfiguration.configuration.logToFile = true;
            startupConfiguration.overrideApplied = true;
        } else if (argument == QStringLiteral("--log-file") && index + 1 < arguments.size()) {
            startupConfiguration.configuration.logFilePath = arguments.at(++index).trimmed();
            startupConfiguration.configuration.logToFile = true;
            startupConfiguration.overrideApplied = true;
        } else if (argument == QStringLiteral("--log-level") && index + 1 < arguments.size()) {
            const QString levelText = arguments.at(++index).trimmed();
            if (applyLevelText(startupConfiguration.configuration, levelText)) {
                startupConfiguration.overrideApplied = true;
            } else {
                startupConfiguration.ignoredLogLevelTexts.push_back(levelText);
            }
        } else if (argument.startsWith(QStringLiteral("--log-level="))) {
            const QString levelText = argument.mid(QStringLiteral("--log-level=").size()).trimmed();
            if (applyLevelText(startupConfiguration.configuration, levelText)) {
                startupConfiguration.overrideApplied = true;
            } else {
                startupConfiguration.ignoredLogLevelTexts.push_back(levelText);
            }
        }
    }

    return startupConfiguration;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (hasArgument(argc, argv, "--version")) {
        std::printf(
            "SkyGate %s (git %s, Qt %s)\n",
            SKYGATE_APP_VERSION,
            SKYGATE_GIT_HASH,
            qVersion()
        );
        return EXIT_SUCCESS;
    }

    // Prevent stale cached QML artifacts from surfacing outdated warnings at launch.
#if !defined(NDEBUG)
    qputenv("QML_DISABLE_DISK_CACHE", "1");
#endif
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);
    QFont::insertSubstitution(QStringLiteral("Sans Serif"), QStringLiteral("Avenir Next"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Menlo"));
    app.setFont(QFont(QStringLiteral("Avenir Next")));
    QCoreApplication::setOrganizationName("Skygate");
    QCoreApplication::setOrganizationDomain("skygate.app");
    QCoreApplication::setApplicationName("SkyGate");
    QCoreApplication::setApplicationVersion(QStringLiteral(SKYGATE_APP_VERSION));
    const StartupLoggingConfiguration startupLogging =
        startupLoggingConfiguration(app.arguments());
    const skygate::ui::SkyLoggingConfiguration loggingConfiguration =
        startupLogging.configuration;
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
    qCInfo(skygateAppLog).noquote() << QStringLiteral(
        "SkyGate started: version=%1 qt=%2 os=%3 %4 overrides=%5"
    ).arg(
        QCoreApplication::applicationVersion(),
        QString::fromLatin1(qVersion()),
        QSysInfo::prettyProductName(),
        skygate::ui::SkyLogging::configurationSummary(skygate::ui::SkyLogging::configuration()),
        startupLogging.overrideApplied ? QStringLiteral("yes") : QStringLiteral("no")
    );
    for (const QString& ignoredLogLevelText : startupLogging.ignoredLogLevelTexts) {
        qCWarning(skygateAppLog).noquote()
            << "Ignoring invalid log level" << ignoredLogLevelText;
    }
    SkySceneModel skySceneModel;
    skySceneModel.setSkyContextController(&skyContextController);
    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &app,
        [&skyContextController] {
            qCInfo(skygateAppLog) << "SkyGate shutting down";
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
