#pragma once

#include "skygate/ephemeris/ObservationEventCalculator.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace skygate::ephemeris {

class IEphemerisEngine;

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

class NightConditionsCalculator final {
public:
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t sunBodyIndex,
        std::uint32_t moonBodyIndex
    ) const;
};

}  // namespace skygate::ephemeris
