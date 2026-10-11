#pragma once

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>

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
};
