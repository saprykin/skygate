#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <memory>
#include <vector>

namespace skygate::ui::internal {

struct SkyCatalogSourceRestoreEntry final {
    SkyCatalogSourceRecord record;
    SkyCatalogSourceInstance instance;
    QByteArray payload;
    bool requiresBinaryUpgrade = false;
};

struct SkyCatalogCollectionRestoreResult final {
    std::vector<SkyCatalogSourceRestoreEntry> sources;
    // True when a stored collection snapshot was applied, including an
    // intentionally empty one, so it is distinguishable from the state where
    // no collection was ever stored.
    bool restored = false;
    bool migratedLegacy = false;
    // The persisted records predate the stored parse contract, so they were
    // restored from the defined defaults (no schema hint, no attribution).
    // The manager rewrites them in the current format to cross the boundary
    // once instead of re-deriving options on every start.
    bool requiresRecordUpgrade = false;
    QString statusText;
};

struct SkyCatalogSourcePersistEntry final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QString url;
    QStringList urls;
    QStringList relatedDatasetUrls;
    QString archiveSelector;
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString attribution;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
    // Marks a record whose catalog is reconstructed from the bundled factory
    // instead of a payload sidecar.
    bool bundled = false;
    const skygate::ephemeris::IStarCatalog* catalog = nullptr;
    QByteArray payload;
    QByteArray constellationLineRows;
    QByteArray constellationAnchorGroupRows;
    int constellationLineSchemaVersion = 0;
    std::size_t constellationCount = 0;
};

struct SkyCatalogCollectionPersistRequest final {
    std::vector<SkyCatalogSourcePersistEntry> sources;
};

// Persists and restores the catalog source collection.
//
// Durable configuration is stored in QSettings as a versioned snapshot while
// raw payloads and binary snapshots are disposable per-instance sidecar
// files. A stored snapshot with no sources is a committed, intentionally
// empty collection, distinct from "no collection was ever stored" (a nullopt
// restore). The legacy two-slot cache is read only to migrate existing
// installations once, and that data stays readable until the new
// configuration is committed. Related constellation datasets are restored per
// owning source before the active related view is composed.
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
    decodeSourceCatalog(const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord, int binarySchemaVersion) const;
    void appendRestoredSource(
        const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord,
        DecodedCatalog decoded,
        RelatedDataOwnership relatedDataOwnership,
        SkyCatalogCollectionRestoreResult& result
    ) const;
    // Decides whether a stored snapshot's related payloads identify their
    // records as owners or may hold copies of the collection-wide view.
    [[nodiscard]] static RelatedDataOwnership
    relatedDataOwnership(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot);
    [[nodiscard]] SkyCatalogCollectionRestoreResult
    restoreFromRecords(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot) const;
    // Converts the legacy two-slot cache into source records. Retained so
    // existing installations keep their saved catalog data; the collection
    // format is authoritative for new writes.
    [[nodiscard]] SkyCatalogCollectionRestoreResult migrateLegacy(
        const SkySettingsStore::CatalogCacheSnapshot& legacy,
        int catalogPresetIndex,
        int deepSkyCatalogPresetIndex,
        const QString& catalogUrlText,
        const QString& deepSkyCatalogUrlText
    ) const;

private:
    SkySettingsStore* m_settingsStore = nullptr;
};

}  // namespace skygate::ui::internal
