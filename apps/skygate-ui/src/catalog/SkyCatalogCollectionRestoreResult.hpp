#pragma once

#include "SkyCatalogSourceRestoreEntry.hpp"

#include <QString>

#include <vector>

namespace skygate::ui::internal {

struct SkyCatalogCollectionRestoreResult final {
    std::vector<SkyCatalogSourceRestoreEntry> sources;
    // True when a stored collection snapshot was applied, including an
    // intentionally empty one, so it is distinguishable from the state where
    // no collection was ever stored.
    bool restored = false;
    bool migratedLegacy = false;
    // A committed collection exists but cannot be read, so the restore was
    // rejected instead of being treated as first use. The caller keeps its
    // active collection and statusText carries the actionable reason; the
    // retired legacy cache is deliberately not migrated over it.
    bool unusableCommittedCollection = false;
    // The persisted records predate the stored parse contract, so they were
    // restored from the defined defaults (no schema hint, no attribution).
    // The manager rewrites them in the current format to cross the boundary
    // once instead of re-deriving options on every start.
    bool requiresRecordUpgrade = false;
    // Reason for a rejected restore of an unreadable committed collection;
    // empty for every other outcome.
    QString statusText;
};

}  // namespace skygate::ui::internal
