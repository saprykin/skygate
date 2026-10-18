#pragma once

#include "SkyEphemerisDataCacheSnapshot.hpp"

namespace skygate::ui::internal {

// Normalizes an installed-data snapshot loaded from settings or produced by
// staged activation: trims metadata, resolves the legacy short-range profile
// id, and restores the default revision and result markers for blank values.
class SkyEphemerisDataCacheSnapshotPolicy final {
public:
    SkyEphemerisDataCacheSnapshotPolicy() = delete;

    [[nodiscard]] static SkyEphemerisDataCacheSnapshot normalizedSnapshot(SkyEphemerisDataCacheSnapshot snapshot);
};

}  // namespace skygate::ui::internal
