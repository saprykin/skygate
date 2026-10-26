#pragma once

#include <cstdint>

namespace skygate::core {

enum class ProjectionStatus : std::uint8_t {
    Culled,
    Visible,
    InvalidCoordinate,
    InvalidParameters
};

}  // namespace skygate::core
