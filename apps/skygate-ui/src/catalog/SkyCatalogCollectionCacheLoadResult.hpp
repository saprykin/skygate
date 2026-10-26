#pragma once

#include "SkyCatalogCollectionCacheSnapshot.hpp"

#include <QString>

#include <cstdint>

// Explicit outcome of reading the committed collection. A snapshot alone
// cannot distinguish first use from committed configuration that was lost
// or corrupted.
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

    State state = State::Absent;
    SkyCatalogCollectionCacheSnapshot snapshot;
    // Human-readable context for an unusable result or for uncommitted
    // generation data that was ignored; empty for a clean result.
    QString diagnostic;

    [[nodiscard]] bool isLoaded() const
    {
        return state == State::Loaded;
    }
};
