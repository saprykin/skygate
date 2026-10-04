#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <memory>
#include <optional>
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
    bool restored = false;
    bool migratedLegacy = false;
    QString statusText;
    std::vector<skygate::ephemeris::ConstellationLineRef> constellationLineRefs;
    std::vector<skygate::ephemeris::ConstellationAnchorGroup> constellationAnchorGroups;
    std::optional<std::size_t> constellationCount;
    bool resetConstellationLineRefs = false;
};

struct SkyCatalogSourcePersistEntry final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QStringList urls;
    QStringList relatedDatasetUrls;
    QString archiveSelector;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
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

class SkyCatalogCacheController final {
public:
    explicit SkyCatalogCacheController(SkySettingsStore* settingsStore);

    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
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
    struct DecodedCatalog final {
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
        bool requiresBinaryUpgrade = false;
    };

    [[nodiscard]] DecodedCatalog
    decodeSourceCatalog(const QByteArray& payload, const QByteArray& binaryPayload, int binarySchemaVersion) const;
    void appendRestoredSource(
        const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord,
        DecodedCatalog decoded,
        SkyCatalogCollectionRestoreResult& result
    ) const;
    [[nodiscard]] SkyCatalogCollectionRestoreResult
    restoreFromRecords(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot) const;
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
