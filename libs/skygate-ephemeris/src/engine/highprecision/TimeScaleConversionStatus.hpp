#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class TimeScaleConversionStatus : std::uint8_t {
    Valid,
    Degraded,
    Failed
};

}  // namespace skygate::ephemeris
