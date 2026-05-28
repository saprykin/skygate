#pragma once

#include "NightConditions.hpp"

#include <cstdint>

namespace skygate::ephemeris {

class BaseCelestialBody;
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
        const BaseCelestialBody* sunBody,
        std::uint32_t moonBodyIndex,
        const BaseCelestialBody* moonBody,
        EventSearchMode eventSearchMode = EventSearchMode::Approximate
    ) const;
};

}  // namespace skygate::ephemeris
