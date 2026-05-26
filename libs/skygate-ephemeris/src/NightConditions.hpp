#pragma once

#include "ObservationEvent.hpp"

#include <optional>
#include <string>

namespace skygate::ephemeris {

struct NightConditions {
    bool valid = false;
    std::optional<double> sunAltitudeDeg;
    ObservationEvent sunrise;
    ObservationEvent sunset;
    ObservationEvent civilDawn;
    ObservationEvent civilDusk;
    ObservationEvent nauticalDawn;
    ObservationEvent nauticalDusk;
    ObservationEvent astronomicalDawn;
    ObservationEvent astronomicalDusk;
    ObservationEvent moonrise;
    ObservationEvent moonset;
    double moonIlluminationPercent = 0.0;
    std::string moonPhaseName;
};

}  // namespace skygate::ephemeris
