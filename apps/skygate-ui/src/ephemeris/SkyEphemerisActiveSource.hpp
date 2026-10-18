#pragma once

#include <cstdint>

namespace skygate::ui::internal {

// Where the active ephemeris data comes from: the installed cache, the
// bundled fallback, or the bundled fallback after installed files went
// missing.
enum class SkyEphemerisActiveSource : std::uint8_t {
    BundledFallback,
    Installed,
    MissingInstalledFallback,
};

}  // namespace skygate::ui::internal
