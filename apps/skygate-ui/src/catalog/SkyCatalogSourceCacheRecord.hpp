#pragma once

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>

namespace skygate::ui::internal {
struct SkyCatalogSourceDescriptor;
struct SkyCatalogSourcePersistEntry;
}  // namespace skygate::ui::internal

// One persisted catalog source. instanceId is the durable identity; the
// remaining fields reconstruct the configured source without re-downloading
// it. payload/binaryPayload are written to sidecar cache files rather than
// stored inline, so the QSettings file stays small.
struct SkyCatalogSourceCacheRecord final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QString url;
    QStringList urls;
    QStringList relatedDatasetUrls;
    QString archiveSelector;
    // Parse contract and descriptor metadata required to reproduce the
    // source. Records written before these fields existed load with the
    // defaults: no schema hint, no attribution.
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString attribution;
    skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bool enabled = true;
    // The source's catalog comes from the bundled factory rather than a
    // stored payload, so the record is configuration only.
    bool bundled = false;
    int order = 0;
    QByteArray payload;
    QByteArray binaryPayload;
    QByteArray constellationLineRows;
    QByteArray constellationAnchorGroupRows;
    int constellationLineSchemaVersion = 0;
    std::size_t constellationCount = 0;

    // The stored record for a source configured from a preset descriptor. The
    // descriptor supplies the configuration; the identity is the one a source
    // adopted from the legacy two-slot cache, because a descriptor carries no
    // durable ID of its own.
    [[nodiscard]] static SkyCatalogSourceCacheRecord fromDescriptor(
        const skygate::ui::internal::SkyCatalogSourceDescriptor& descriptor,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );

    // The stored record for one persist entry. A bundled source keeps its
    // configuration only; every other source stores the entry's payload and
    // the binary sidecar serialized from the catalog it borrows, which stays
    // owned by the caller.
    [[nodiscard]] static SkyCatalogSourceCacheRecord
    fromPersistEntry(const skygate::ui::internal::SkyCatalogSourcePersistEntry& entry);
};
