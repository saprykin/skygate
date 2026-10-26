#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Each enumerator carries its own mask value.
enum class EphemerisCapability : std::uint8_t {
    SolarSystemBodies = 1U << 0U,
    CatalogStars = 1U << 1U,
    TopocentricPositions = 1U << 2U,
    AtmosphericRefraction = 1U << 3U,
    ExtendedHistoricalRange = 1U << 4U
};

}  // namespace skygate::ephemeris
