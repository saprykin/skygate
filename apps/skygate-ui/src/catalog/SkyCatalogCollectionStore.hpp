#pragma once

#include "SkyCatalogCollectionCacheLoadResult.hpp"
#include "SkyCatalogCollectionCacheSnapshot.hpp"

#include <QString>

namespace skygate::ui::internal {

// Owns the durable catalog collection: the generation manifest, the
// generation-scoped source records in QSettings, and the per-instance payload
// sidecar files. A save stages a complete new generation and publishes the
// manifest last, so an interrupted or failed save leaves the committed
// collection loadable. Callers decide how a loaded snapshot is validated and
// migrated; this class only reads and writes what was committed.
class SkyCatalogCollectionStore final {
public:
    // Record format written by save(). Version 3 stores each source record's
    // own related constellation dataset; version 2 records copied the composed
    // collection-wide view into every eligible source, and version 1 records
    // carry no parse contract. An older record loads with the defined defaults
    // and the caller rewrites it in the current format.
    static constexpr int kCollectionSchemaVersion = 3;
    // Related constellation datasets on disk carry their own format version.
    static constexpr int kConstellationLineCacheSchemaVersion = 4;

    [[nodiscard]] bool save(const SkyCatalogCollectionCacheSnapshot& snapshot) const;
    [[nodiscard]] SkyCatalogCollectionCacheLoadResult load() const;
    [[nodiscard]] bool clear() const;
    // Evicts the source's disposable payload files and the payload references
    // in its record while every configuration field stays durable: identity,
    // order, enabled state, descriptor, and parse contract. A restart restores
    // the source as configured with an unavailable payload; only a later
    // accepted load writes payload references again. Removing the source stays
    // the separate configuration-deletion operation.
    [[nodiscard]] bool evictSource(const QString& instanceId) const;
};

}  // namespace skygate::ui::internal
