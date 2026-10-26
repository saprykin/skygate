#pragma once

#include "SkyCatalogSourceCacheRecord.hpp"

#include <QVector>

// Saved even when sources are empty: a stored snapshot is a committed
// configuration, so an intentionally empty collection stays distinct from
// "no collection was ever committed" (the Absent load state).
//
// A save stages a new generation and publishes it only after every payload
// file and record is durable, so a failed save leaves the previously
// committed snapshot loadable instead of mixing old records with new
// payloads.
struct SkyCatalogCollectionCacheSnapshot final {
    int schemaVersion = 0;
    int binarySchemaVersion = 0;
    QVector<SkyCatalogSourceCacheRecord> sources;
};
