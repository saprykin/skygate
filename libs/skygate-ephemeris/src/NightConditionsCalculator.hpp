#pragma once

#include "ObservationEventCalculator.hpp"

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

enum class NightConditionsEventSearchMode : std::uint8_t {
    Approximate,
    Verified
};

class NightConditionsCalculator final {
public:
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t sunBodyIndex,
        std::uint32_t moonBodyIndex
    ) const;
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const core::SkyContext& context,
        std::uint32_t sunBodyIndex,
        const CelestialBody& sunBody,
        std::uint32_t moonBodyIndex,
        const CelestialBody& moonBody
    ) const;
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        std::uint32_t moonBodyIndex
    ) const;
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        const CelestialBody& sunBody,
        std::uint32_t moonBodyIndex,
        const CelestialBody& moonBody
    ) const;
    // Approximate mode uses simple-engine event estimates for high-precision
    // requests. Verified mode samples the selected engine directly for event
    // times and should be used when night-condition event times are audited.
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        const CelestialBody& sunBody,
        std::uint32_t moonBodyIndex,
        const CelestialBody& moonBody,
        NightConditionsEventSearchMode eventSearchMode
    ) const;

private:
    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        const CelestialBody* sunBody,
        std::uint32_t moonBodyIndex,
        const CelestialBody* moonBody,
        NightConditionsEventSearchMode eventSearchMode
    ) const;
};

}  // namespace skygate::ephemeris
