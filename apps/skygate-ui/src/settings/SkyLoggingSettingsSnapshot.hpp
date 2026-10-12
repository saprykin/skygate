#pragma once

#include <QString>

namespace skygate::ui::internal {

struct SkyLoggingSettingsSnapshot final {
    bool logToTerminal = true;
    bool logToFile = false;
    QString logFilePath;
};

}  // namespace skygate::ui::internal
