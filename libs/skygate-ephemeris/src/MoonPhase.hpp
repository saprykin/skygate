#pragma once

#include <string>

namespace skygate::ephemeris {

struct MoonPhase {
    double illuminationPercent = 0.0;
    std::string phaseName;
};

}  // namespace skygate::ephemeris
