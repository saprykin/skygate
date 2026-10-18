#include "SkyWindowSizeSettings.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QLoggingCategory>
#include <QSettings>
#include <QString>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")

QString appSettingsKey(const char* name)
{
    return QStringLiteral("app/%1").arg(QString::fromUtf8(name));
}

QSize normalizedMainWindowSize(const QSize& size) noexcept
{
    const int width = size.width() >= SkyWindowSizeSettings::kMinimumMainWindowWidth
                          ? size.width()
                          : SkyWindowSizeSettings::kDefaultMainWindowWidth;
    const int height = size.height() >= SkyWindowSizeSettings::kMinimumMainWindowHeight
                           ? size.height()
                           : SkyWindowSizeSettings::kDefaultMainWindowHeight;
    return QSize(width, height);
}

}  // namespace

bool SkyWindowSizeSettings::save(const QSize& size)
{
    QSettings settings;
    const QSize normalizedSize = normalizedMainWindowSize(size);
    settings.setValue(appSettingsKey("mainWindowWidth"), normalizedSize.width());
    settings.setValue(appSettingsKey("mainWindowHeight"), normalizedSize.height());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateSettingsLog) << "Failed to save SkyGate main window size";
        return false;
    }
    return true;
}

QSize SkyWindowSizeSettings::load()
{
    QSettings settings;
    return normalizedMainWindowSize(QSize(
        SkySettingsValueCodecs::readIntSetting(settings, appSettingsKey("mainWindowWidth"), kDefaultMainWindowWidth),
        SkySettingsValueCodecs::readIntSetting(settings, appSettingsKey("mainWindowHeight"), kDefaultMainWindowHeight)
    ));
}

}  // namespace skygate::ui::internal
