#pragma once

#include "SkyPersistedStateSnapshot.hpp"
#include "catalog/SkyCatalogCollectionCacheLoadResult.hpp"
#include "catalog/SkyCatalogCollectionCacheSnapshot.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "catalog/SkyCatalogLegacyCacheSnapshot.hpp"
#include "ephemeris/SkyEphemerisDataCacheSnapshot.hpp"

#include <QSize>
#include <QString>

#include <optional>

class SkySettingsStore final {
public:
    [[nodiscard]] bool saveState(const SkyPersistedStateSnapshot& snapshot) const;
    [[nodiscard]] std::optional<SkyPersistedStateSnapshot> loadState() const;
    [[nodiscard]] bool saveMainWindowSize(const QSize& size) const;
    [[nodiscard]] QSize loadMainWindowSize() const;
    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
    [[nodiscard]] bool saveCatalogCache(const SkyCatalogLegacyCacheSnapshot& snapshot) const;
    [[nodiscard]] std::optional<SkyCatalogLegacyCacheSnapshot> loadCatalogCache() const;
    // Collection persistence is implemented by SkyCatalogCollectionStore;
    // these entry points stay as forwarding methods for existing callers.
    [[nodiscard]] bool saveCatalogCollectionCache(const SkyCatalogCollectionCacheSnapshot& snapshot) const;
    [[nodiscard]] SkyCatalogCollectionCacheLoadResult loadCatalogCollectionCache() const;
    [[nodiscard]] bool clearCatalogCollectionCache() const;
    // Evicts the source's disposable payload files and the payload references
    // in its record while every configuration field stays durable: identity,
    // order, enabled state, descriptor, and parse contract. A restart restores
    // the source as configured with an unavailable payload; only a later
    // accepted load writes payload references again. Removing the source stays
    // the separate configuration-deletion operation.
    [[nodiscard]] bool clearCatalogSourceCache(const QString& instanceId) const;
    [[nodiscard]] bool saveEphemerisDataCache(const SkyEphemerisDataCacheSnapshot& snapshot) const;
    [[nodiscard]] SkyEphemerisDataCacheSnapshot loadEphemerisDataCache() const;
    [[nodiscard]] bool clearEphemerisDataCache() const;

private:
    skygate::ui::internal::SkyCatalogCollectionStore m_catalogCollectionStore;
};
