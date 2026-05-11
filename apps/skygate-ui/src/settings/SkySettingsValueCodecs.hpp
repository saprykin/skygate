#pragma once

#include <QString>
#include <QVariant>
#include <QtGlobal>

#include <optional>

class QSettings;

namespace skygate::ui::internal {

[[nodiscard]] std::optional<bool> boolFromSettingValue(const QVariant& value);
[[nodiscard]] bool readBoolSetting(QSettings& settings, const QString& key, bool fallback);
[[nodiscard]] double readDoubleSetting(QSettings& settings, const QString& key, double fallback);
[[nodiscard]] int readIntSetting(QSettings& settings, const QString& key, int fallback);
[[nodiscard]] qint64 readLongLongSetting(QSettings& settings, const QString& key, qint64 fallback);
[[nodiscard]] qulonglong readULongLongSetting(
    QSettings& settings,
    const QString& key,
    qulonglong fallback
);
[[nodiscard]] QString readNonBlankStringSetting(
    QSettings& settings,
    const QString& key,
    const QString& fallback
);

}  // namespace skygate::ui::internal
