#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class CatalogSelectionMode : std::uint8_t {
    Disabled,
    BrightestByVisualMagnitude
};

}  // namespace skygate::ephemeris
