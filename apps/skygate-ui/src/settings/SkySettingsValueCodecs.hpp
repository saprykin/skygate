#pragma once

#include <QString>
#include <QVariant>
#include <QtGlobal>

#include <optional>

class QSettings;

namespace skygate::ui::internal {

class SkySettingsValueCodecs final {
public:
    [[nodiscard]] static std::optional<bool> boolFromSettingValue(const QVariant& value);
    [[nodiscard]] static bool readBoolSetting(QSettings& settings, const QString& key, bool fallback);
    [[nodiscard]] static double readDoubleSetting(QSettings& settings, const QString& key, double fallback);
    [[nodiscard]] static int readIntSetting(QSettings& settings, const QString& key, int fallback);
    [[nodiscard]] static qint64 readLongLongSetting(QSettings& settings, const QString& key, qint64 fallback);
    [[nodiscard]] static qulonglong readULongLongSetting(QSettings& settings, const QString& key, qulonglong fallback);
    [[nodiscard]] static QString
    readNonBlankStringSetting(QSettings& settings, const QString& key, const QString& fallback);
};

}  // namespace skygate::ui::internal
