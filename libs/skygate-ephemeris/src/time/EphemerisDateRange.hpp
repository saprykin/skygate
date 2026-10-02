#pragma once

#include "time/AstronomicalEpoch.hpp"

#include <string>

namespace skygate::ephemeris {

struct EphemerisDateRange {
    std::string id;
    std::string displayName;
    skygate::core::AstronomicalEpoch start;
    skygate::core::AstronomicalEpoch end;
};

}  // namespace skygate::ephemeris
