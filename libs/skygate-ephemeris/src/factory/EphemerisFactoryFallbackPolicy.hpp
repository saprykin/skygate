#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisFactoryFallbackPolicy : std::uint8_t {
    StrictHighPrecision,
    AllowSimpleEngineFallback
};

}  // namespace skygate::ephemeris
