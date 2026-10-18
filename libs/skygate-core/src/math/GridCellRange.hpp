#pragma once

#include <cstdint>

namespace skygate::core {

struct GridCellRange final {
    std::int32_t minX = 0;
    std::int32_t maxX = 0;
    std::int32_t minY = 0;
    std::int32_t maxY = 0;
};

}  // namespace skygate::core
