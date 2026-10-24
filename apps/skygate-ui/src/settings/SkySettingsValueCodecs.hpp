#pragma once

#include <QString>
#include <QVariant>
#include <QtGlobal>

#include <optional>

class QLoggingCategory;
class QSettings;

namespace skygate::ui::internal {

class SkySettingsValueCodecs final {
public:
    // The accessor a Q_LOGGING_CATEGORY macro declares, so a reader can log
    // through the category of the settings group it belongs to.
    using LoggingCategoryAccessor = const QLoggingCategory& (*)();

    [[nodiscard]] static std::optional<bool> boolFromSettingValue(const QVariant& value);
    // Reads a stored boolean that has no fallback: a missing or unreadable
    // value returns no value, and an unreadable value is reported through the
    // caller's logging category with the caller's note on what happens next.
    [[nodiscard]] static std::optional<bool> readOptionalBoolSetting(
        QSettings& settings, const QString& key, LoggingCategoryAccessor loggingCategory, const char* invalidValueNote
    );
    [[nodiscard]] static bool readBoolSetting(QSettings& settings, const QString& key, bool fallback);
    [[nodiscard]] static double readDoubleSetting(QSettings& settings, const QString& key, double fallback);
    [[nodiscard]] static int readIntSetting(QSettings& settings, const QString& key, int fallback);
    [[nodiscard]] static qint64 readLongLongSetting(QSettings& settings, const QString& key, qint64 fallback);
    [[nodiscard]] static qulonglong readULongLongSetting(QSettings& settings, const QString& key, qulonglong fallback);
    [[nodiscard]] static QString
    readNonBlankStringSetting(QSettings& settings, const QString& key, const QString& fallback);
    // Stores the trimmed value, or the fallback when that leaves nothing to
    // store, the same way readNonBlankStringSetting treats a blank value.
    static void
    writeNonBlankStringSetting(QSettings& settings, const QString& key, const QString& value, const QString& fallback);
};

}  // namespace skygate::ui::internal
