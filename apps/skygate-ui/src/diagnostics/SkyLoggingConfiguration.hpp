#pragma once

#include <QString>
#include <QtGlobal>

namespace skygate::ui {

struct SkyLoggingConfiguration final {
    bool logToTerminal = true;
    bool logToFile = false;
    QString logFilePath;
    qint64 maxFileBytes = 5 * 1024 * 1024;
    int backupFileCount = 3;
    QtMsgType terminalMinimumType = QtInfoMsg;
    QtMsgType fileMinimumType = QtInfoMsg;
};

}  // namespace skygate::ui
