#pragma once

#include "HighPrecisionCalculatorResult.hpp"

#include <cstddef>

namespace skygate::ephemeris::highprecision {

struct StarAstrometryBatchResult {
    std::size_t bodyIndex = 0U;
    HighPrecisionCalculatorResult result;
};

}  // namespace skygate::ephemeris::highprecision
