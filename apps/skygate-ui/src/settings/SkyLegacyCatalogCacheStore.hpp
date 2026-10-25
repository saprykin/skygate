#pragma once

#include "catalog/SkyCatalogLegacyCacheSnapshot.hpp"

#include <optional>

namespace skygate::ui::internal {

// Reads and clears the legacy two-slot catalog cache: a star slot and a
// deep-sky slot, each with a text and a binary payload file. The files keep
// their original names so an older installation still restores. New catalogs
// are persisted by SkyCatalogCollectionStore; this storage exists only for
// restoring and clearing installations that predate it.
class SkyLegacyCatalogCacheStore final {
public:
    [[nodiscard]] static std::optional<SkyCatalogLegacyCacheSnapshot> load();
    [[nodiscard]] static bool clearStarCache();
    [[nodiscard]] static bool clearDeepSkyCache();
};

}  // namespace skygate::ui::internal
