#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class CatalogCompositionSource : std::uint8_t {
    Primary,
    DeepSky,
    BuiltInEphemeris
};

}  // namespace skygate::ephemeris
