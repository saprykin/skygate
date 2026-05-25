#pragma once

#include "NightConditions.hpp"

#include <cstdint>

namespace skygate::ephemeris {

struct CelestialBody;
struct EphemerisRequest;
class IEphemerisEngine;

class NightConditionsCalculator final {
public:
    enum class EventSearchMode : std::uint8_t {
        Approximate,
        Verified
    };

    [[nodiscard]] NightConditions compute(
        const IEphemerisEngine& ephemerisEngine,
        const EphemerisRequest& request,
        std::uint32_t sunBodyIndex,
        const CelestialBody* sunBody,
        std::uint32_t moonBodyIndex,
        const CelestialBody* moonBody,
        EventSearchMode eventSearchMode = EventSearchMode::Approximate
    ) const;
};

}  // namespace skygate::ephemeris
