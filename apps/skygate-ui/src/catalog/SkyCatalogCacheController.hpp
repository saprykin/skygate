#pragma once

#include "SkyCatalogCollectionCacheSnapshot.hpp"
#include "SkyCatalogCollectionPersistRequest.hpp"
#include "SkyCatalogCollectionRestoreResult.hpp"
#include "SkyCatalogLegacyCacheSnapshot.hpp"
#include "SkyCatalogSourceCacheRecord.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <memory>
#include <vector>

class SkySettingsStore;

namespace skygate::ui::internal {

// Persists and restores the catalog source collection.
//
// Durable configuration is stored in QSettings as a versioned snapshot while
// raw payloads and binary snapshots are disposable per-instance sidecar
// files. A stored snapshot with no sources is a committed, intentionally
// empty collection, distinct from "no collection was ever stored" (the Absent
// load state). The legacy two-slot cache is read only to migrate existing
// installations once, and only while no collection was ever committed; a
// committed configuration that cannot be read is reported as a rejected
// restore instead of being replaced by that retired data. Related
// constellation datasets are restored per owning source before the active
// related view is composed.
class SkyCatalogCacheController final {
public:
    explicit SkyCatalogCacheController(SkySettingsStore* settingsStore);

    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
    // Evicts the source's disposable payload while its configured record
    // stays durable; the accepted in-memory snapshot remains active until a
    // reload or restart.
    [[nodiscard]] bool clearSourceCache(const QString& instanceId) const;
    [[nodiscard]] bool clearCollectionCache() const;
    [[nodiscard]] SkyCatalogCollectionRestoreResult restoreCollection(
        int catalogPresetIndex,
        int deepSkyCatalogPresetIndex,
        const QString& catalogUrlText,
        const QString& deepSkyCatalogUrlText
    ) const;
    void persistCollection(const SkyCatalogCollectionPersistRequest& request) const;

private:
    // How a restored source record's related constellation payload may be
    // attributed. Records written before the per-source related format stored
    // a copy of the composed collection-wide dataset instead of the owning
    // source's own dataset, so their rows establish an owner only when the
    // snapshot holds a single such record.
    enum class RelatedDataOwnership {
        SourceOwned,
        AmbiguousLegacyCopy,
    };

    struct DecodedCatalog final {
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
        bool requiresBinaryUpgrade = false;
    };

    [[nodiscard]] DecodedCatalog
    decodeSourceCatalog(const SkyCatalogSourceCacheRecord& sourceRecord, int binarySchemaVersion) const;
    void appendRestoredSource(
        const SkyCatalogSourceCacheRecord& sourceRecord,
        DecodedCatalog decoded,
        RelatedDataOwnership relatedDataOwnership,
        SkyCatalogCollectionRestoreResult& result
    ) const;
    // Decides whether a stored snapshot's related payloads identify their
    // records as owners or may hold copies of the collection-wide view.
    [[nodiscard]] static RelatedDataOwnership relatedDataOwnership(const SkyCatalogCollectionCacheSnapshot& snapshot);
    [[nodiscard]] SkyCatalogCollectionRestoreResult
    restoreFromRecords(const SkyCatalogCollectionCacheSnapshot& snapshot) const;
    // Converts the legacy two-slot cache into source records. Retained so
    // existing installations keep their saved catalog data; the collection
    // format is authoritative for new writes.
    [[nodiscard]] SkyCatalogCollectionRestoreResult migrateLegacy(
        const SkyCatalogLegacyCacheSnapshot& legacy,
        int catalogPresetIndex,
        int deepSkyCatalogPresetIndex,
        const QString& catalogUrlText,
        const QString& deepSkyCatalogUrlText
    ) const;

private:
    SkySettingsStore* m_settingsStore = nullptr;
};

}  // namespace skygate::ui::internal
