#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class ObservationEventStatus : std::uint8_t {
    Available,
    AlwaysAbove,
    AlwaysBelow,
    NoEventInSearchWindow,
    InvalidInput,
    Unresolved
};

}  // namespace skygate::ephemeris
