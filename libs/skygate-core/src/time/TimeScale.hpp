#pragma once

#include <cstdint>

namespace skygate::core {

enum class TimeScale : std::uint8_t {
    Utc,
    Tai,
    Tt,
    Tdb,
    Ut1
};

}  // namespace skygate::core
