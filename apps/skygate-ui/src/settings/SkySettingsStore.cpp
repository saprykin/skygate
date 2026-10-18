#include "SkySettingsStore.hpp"
#include "SkyEphemerisDataCacheSettings.hpp"
#include "SkyLegacyCatalogCacheStore.hpp"
#include "SkySettingsSnapshotCodecs.hpp"
#include "SkyWindowSizeSettings.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"

#include <QLoggingCategory>
#include <QSettings>

#include <optional>

using namespace skygate::ui::internal;

namespace {

Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")

}  // namespace

bool SkySettingsStore::saveState(const SkyPersistedStateSnapshot& snapshot) const
{
    QSettings settings;
    SkySettingsSnapshotCodecs::saveStateSnapshot(settings, snapshot);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateSettingsLog) << "Failed to save SkyGate settings";
        return false;
    }
    return true;
}

std::optional<SkyPersistedStateSnapshot> SkySettingsStore::loadState() const
{
    QSettings settings;
    return SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
}

bool SkySettingsStore::saveMainWindowSize(const QSize& size) const
{
    return SkyWindowSizeSettings::save(size);
}

QSize SkySettingsStore::loadMainWindowSize() const
{
    return SkyWindowSizeSettings::load();
}

SkyLoggingSettingsSnapshot SkySettingsStore::loadLoggingSettings() const
{
    QSettings settings;
    return SkySettingsSnapshotCodecs::loadLoggingSettings(settings);
}

bool SkySettingsStore::clearCatalogCache() const
{
    return SkyLegacyCatalogCacheStore::clearStarCache();
}

bool SkySettingsStore::clearDeepSkyCatalogCache() const
{
    return SkyLegacyCatalogCacheStore::clearDeepSkyCache();
}

bool SkySettingsStore::saveCatalogCache(const SkyCatalogLegacyCacheSnapshot& snapshot) const
{
    return SkyLegacyCatalogCacheStore::save(snapshot);
}

std::optional<SkyCatalogLegacyCacheSnapshot> SkySettingsStore::loadCatalogCache() const
{
    return SkyLegacyCatalogCacheStore::load();
}

bool SkySettingsStore::saveCatalogCollectionCache(const SkyCatalogCollectionCacheSnapshot& snapshot) const
{
    return m_catalogCollectionStore.save(snapshot);
}

SkyCatalogCollectionCacheLoadResult SkySettingsStore::loadCatalogCollectionCache() const
{
    return m_catalogCollectionStore.load();
}

bool SkySettingsStore::clearCatalogCollectionCache() const
{
    return m_catalogCollectionStore.clear();
}

bool SkySettingsStore::clearCatalogSourceCache(const QString& instanceId) const
{
    return m_catalogCollectionStore.evictSource(instanceId);
}

bool SkySettingsStore::saveEphemerisDataCache(const SkyEphemerisDataCacheSnapshot& snapshot) const
{
    return SkyEphemerisDataCacheSettings::save(snapshot);
}

SkyEphemerisDataCacheSnapshot SkySettingsStore::loadEphemerisDataCache() const
{
    return SkyEphemerisDataCacheSettings::load();
}

bool SkySettingsStore::clearEphemerisDataCache() const
{
    return SkyEphemerisDataCacheSettings::clear();
}
