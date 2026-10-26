#pragma once

#include "ObservationEvent.hpp"

#include <string>

namespace skygate::ephemeris {

struct NightConditions {
    bool valid = false;
    ObservationEvent sunrise;
    ObservationEvent sunset;
    ObservationEvent civilDusk;
    ObservationEvent nauticalDusk;
    ObservationEvent astronomicalDawn;
    ObservationEvent astronomicalDusk;
    ObservationEvent moonrise;
    ObservationEvent moonset;
    double moonIlluminationPercent = 0.0;
    std::string moonPhaseName;
};

}  // namespace skygate::ephemeris
