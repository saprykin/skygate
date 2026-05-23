#pragma once

#include "GeoLocation.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::core {

struct SkyContext {
    GeoLocation observer;
    UtcTimePoint utcTime{};
};

}  // namespace skygate::core
