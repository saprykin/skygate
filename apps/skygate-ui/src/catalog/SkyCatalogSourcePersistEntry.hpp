#pragma once

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogSourceRecord.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>

namespace skygate::ui::internal {

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

    // The entry the persist path starts from for a source the runtime holds.
    // Only the configuration the runtime record owns is copied: accepted
    // facts, the payload, and the borrowed catalog pointer are decided by the
    // caller that knows the source's operation state.
    [[nodiscard]] static SkyCatalogSourcePersistEntry fromRuntimeRecord(const SkyCatalogSourceRecord& source)
    {
        SkyCatalogSourcePersistEntry entry;
        entry.instanceId = source.instanceId;
        entry.title = source.title;
        entry.version = source.version;
        entry.url = source.url;
        entry.policy = source.policy;
        entry.enabled = source.enabled;
        entry.bundled = source.bundled;
        return entry;
    }
};

}  // namespace skygate::ui::internal
