#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace skygate::ui {

struct SkyLoggingConfiguration final {
    bool logToTerminal = true;
    bool logToFile = false;
    QString logFilePath;
    qint64 maxFileBytes = 5 * 1024 * 1024;
    int backupFileCount = 3;
    QtMsgType terminalMinimumType = QtWarningMsg;
    QtMsgType fileMinimumType = QtInfoMsg;
};

class SkyLogging final {
public:
    [[nodiscard]] static SkyLoggingConfiguration defaultConfiguration();
    [[nodiscard]] static QString defaultLogFilePath();
    [[nodiscard]] static SkyLoggingConfiguration configuration();
    [[nodiscard]] static bool isInstalled();

    static void install(const SkyLoggingConfiguration& configuration);
    static void configure(const SkyLoggingConfiguration& configuration);
    static void uninstall();

private:
    SkyLogging() = default;
};

}  // namespace skygate::ui
