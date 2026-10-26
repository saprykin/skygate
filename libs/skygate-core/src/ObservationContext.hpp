#pragma once

#include "GeoLocation.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::core {

struct ObservationContext {
    GeoLocation observer;
    UtcTimePoint utcTime{};
};

}  // namespace skygate::core
