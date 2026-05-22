#pragma once

#include "skygate/core/GeoLocation.hpp"
#include "skygate/core/UtcTimePoint.hpp"

namespace skygate::core {

struct SkyContext {
    GeoLocation observer;
    UtcTimePoint utcTime{};
};

}  // namespace skygate::core
