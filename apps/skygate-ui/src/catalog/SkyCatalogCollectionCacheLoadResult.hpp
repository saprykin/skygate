#pragma once

#include "SkyCatalogCollectionCacheSnapshot.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>

// Explicit outcome of reading the committed collection. A snapshot alone
// cannot distinguish first use from committed configuration that was lost
// or corrupted, nor a deliberately evicted payload from unreadable
// committed data.
struct SkyCatalogCollectionCacheLoadResult final {
    enum class State : std::uint8_t {
        // No collection was ever committed, so a legacy cache may still
        // be migrated by the caller.
        Absent,
        // A committed collection was applied; an empty one is valid.
        Loaded,
        // A committed collection exists but cannot be read.
        Unusable,
    };

    enum class Failure : std::uint8_t {
        None,
        // A durable commit record proves a collection was committed, but
        // its manifest is gone.
        CommittedManifestMissing,
        // The manifest file exists but is incomplete or truncated.
        ManifestUnreadable,
        // The manifest names a generation whose stored records are gone.
        CommittedGenerationMissing,
        // Only the pre-generation version marker survives; its records
        // were replaced by a generation that was never committed.
        LegacyRecordsReplaced,
    };

    State state = State::Absent;
    Failure failure = Failure::None;
    SkyCatalogCollectionCacheSnapshot snapshot;
    // Human-readable context for an unusable result or for uncommitted
    // generation data that was ignored; empty for a clean result.
    QString diagnostic;
    // Records that reference a payload sidecar which is missing or
    // unreadable. A deliberately evicted payload keeps no reference and is
    // not listed here.
    QStringList unreadablePayloadInstanceIds;

    [[nodiscard]] bool isLoaded() const
    {
        return state == State::Loaded;
    }
};
