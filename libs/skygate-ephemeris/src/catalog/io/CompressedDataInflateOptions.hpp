#pragma once

#include <cstddef>

namespace skygate::ephemeris {

struct CompressedDataInflateOptions final {
    std::size_t expectedOutputBytes = 0;
    bool allowEmptyOutput = false;
};

}  // namespace skygate::ephemeris
