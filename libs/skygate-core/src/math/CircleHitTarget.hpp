#pragma once

#include <cstdint>

namespace skygate::core {

struct CircleHitTarget final {
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
    std::uint32_t payloadId = 0;
};

}  // namespace skygate::core
