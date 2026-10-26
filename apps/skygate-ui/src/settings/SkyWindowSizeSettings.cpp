#include "SkyWindowSizeSettings.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"

#include <QLoggingCategory>
#include <QSettings>

namespace skygate::ui::internal {
namespace {

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
    settings.setValue(SkySettingsKeyBuilder::appKey("mainWindowWidth"), normalizedSize.width());
    settings.setValue(SkySettingsKeyBuilder::appKey("mainWindowHeight"), normalizedSize.height());
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
        SkySettingsValueCodecs::readIntSetting(
            settings, SkySettingsKeyBuilder::appKey("mainWindowWidth"), kDefaultMainWindowWidth
        ),
        SkySettingsValueCodecs::readIntSetting(
            settings, SkySettingsKeyBuilder::appKey("mainWindowHeight"), kDefaultMainWindowHeight
        )
    ));
}

}  // namespace skygate::ui::internal
