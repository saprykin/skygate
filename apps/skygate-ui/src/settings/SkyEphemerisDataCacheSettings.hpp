#pragma once

#include "ephemeris/SkyEphemerisDataCacheSnapshot.hpp"

namespace skygate::ui::internal {

// Durable metadata for the installed ephemeris data: the kernel, Earth
// orientation, leap second, and delta-T assets with their versions, plus the
// revision token and the outcome text of the last update. The payload files
// themselves are managed by the ephemeris cache controller.
class SkyEphemerisDataCacheSettings final {
public:
    [[nodiscard]] static bool save(const SkyEphemerisDataCacheSnapshot& snapshot);
    [[nodiscard]] static SkyEphemerisDataCacheSnapshot load();
    [[nodiscard]] static bool clear();
};

}  // namespace skygate::ui::internal
