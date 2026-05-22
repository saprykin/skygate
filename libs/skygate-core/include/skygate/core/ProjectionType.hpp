#pragma once

#include <cstdint>

namespace skygate::core {

enum class ProjectionType : std::uint8_t {
    Stereographic,
    AzimuthalEquidistant,
    Perspective
};

}  // namespace skygate::core
