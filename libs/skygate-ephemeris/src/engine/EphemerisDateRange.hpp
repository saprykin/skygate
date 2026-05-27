#pragma once

#include "time/AstronomicalEpoch.hpp"

#include <string>

namespace skygate::ephemeris {

struct EphemerisDateRange {
    std::string id;
    std::string displayName;
    AstronomicalEpoch start;
    AstronomicalEpoch end;
};

}  // namespace skygate::ephemeris
