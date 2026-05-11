#include "SkySettingsValueCodecs.hpp"

#include <QLoggingCategory>
#include <QSettings>
#include <QString>

#include <cmath>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")

}  // namespace

std::optional<bool> boolFromSettingValue(const QVariant& value)
{
    if (!value.isValid()) {
        return std::nullopt;
    }
    if (value.metaType().id() == QMetaType::Bool) {
        return value.toBool();
    }

    const QString text = value.toString().trimmed().toLower();
    if (text == "true" || text == "1" || text == "yes" || text == "on") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no" || text == "off") {
        return false;
    }
    return std::nullopt;
}

bool readBoolSetting(QSettings& settings, const QString& key, const bool fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    const QVariant value = settings.value(key);
    const std::optional<bool> parsedValue = boolFromSettingValue(value);
    if (!parsedValue.has_value()) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid boolean setting" << key << "value" << value.toString()
            << "- using fallback" << fallback;
        return fallback;
    }
    return *parsedValue;
}

double readDoubleSetting(QSettings& settings, const QString& key, const double fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const double value = settings.value(key).toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid numeric setting" << key << "value" << settings.value(key).toString()
            << "- using fallback" << fallback;
        return fallback;
    }
    return value;
}

int readIntSetting(QSettings& settings, const QString& key, const int fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const int value = settings.value(key).toInt(&ok);
    if (!ok) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid integer setting" << key << "value" << settings.value(key).toString()
            << "- using fallback" << fallback;
        return fallback;
    }
    return value;
}

qint64 readLongLongSetting(QSettings& settings, const QString& key, const qint64 fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const qint64 value = settings.value(key).toLongLong(&ok);
    if (!ok) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid integer setting" << key << "value" << settings.value(key).toString()
            << "- using fallback" << fallback;
        return fallback;
    }
    return value;
}

qulonglong readULongLongSetting(
    QSettings& settings,
    const QString& key,
    const qulonglong fallback
)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const qulonglong value = settings.value(key).toULongLong(&ok);
    if (!ok) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid unsigned integer setting" << key << "value" << settings.value(key).toString()
            << "- using fallback" << fallback;
        return fallback;
    }
    return value;
}

QString readNonBlankStringSetting(
    QSettings& settings,
    const QString& key,
    const QString& fallback
)
{
    const QString configuredValue = settings.value(key, fallback).toString().trimmed();
    if (configuredValue.isEmpty()) {
        qCWarning(skygateSettingsLog).noquote()
            << "Invalid blank log file path setting" << key << "- using fallback" << fallback;
        return fallback;
    }
    return configuredValue;
}

}  // namespace skygate::ui::internal
