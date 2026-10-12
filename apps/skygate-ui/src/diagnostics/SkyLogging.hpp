#pragma once

#include "SkyLoggingConfiguration.hpp"

#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace skygate::ui {

class SkyLogging final {
public:
    [[nodiscard]] static SkyLoggingConfiguration defaultConfiguration();
    [[nodiscard]] static QString defaultLogFilePath();
    [[nodiscard]] static SkyLoggingConfiguration configuration();
    [[nodiscard]] static std::optional<QtMsgType> messageTypeFromLevelText(const QString& levelText);
    [[nodiscard]] static QString levelSummary(QtMsgType type);
    [[nodiscard]] static QString outputSummary(const SkyLoggingConfiguration& configuration);
    [[nodiscard]] static QString configurationSummary(const SkyLoggingConfiguration& configuration);
    [[nodiscard]] static bool isInstalled();

    static void install(const SkyLoggingConfiguration& configuration);
    static void configure(const SkyLoggingConfiguration& configuration);
    static void uninstall();

private:
    SkyLogging() = default;
};

}  // namespace skygate::ui
