#pragma once

#include <string_view>

namespace skygate::ephemeris {

// Canonical body ids of the solar-system bodies the bundled catalog defines.
class CelestialBodyIds final {
public:
    CelestialBodyIds() = delete;

    static constexpr std::string_view kSun = "sun";
    static constexpr std::string_view kMoon = "moon";
    static constexpr std::string_view kMercury = "mercury";
    static constexpr std::string_view kVenus = "venus";
    static constexpr std::string_view kMars = "mars";
    static constexpr std::string_view kJupiter = "jupiter";
    static constexpr std::string_view kSaturn = "saturn";
    static constexpr std::string_view kUranus = "uranus";
    static constexpr std::string_view kNeptune = "neptune";
};

}  // namespace skygate::ephemeris
