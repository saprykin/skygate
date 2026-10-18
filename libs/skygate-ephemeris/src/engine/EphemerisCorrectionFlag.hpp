#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Each enumerator carries its own mask value. The last entries combine several
// flags into the presets the settings UI offers.
enum class EphemerisCorrectionFlag : std::uint32_t {
    NoCorrections = 0U,
    Geometric = 1U << 11U,
    LightTime = 1U << 0U,
    StellarAberration = 1U << 1U,
    GravitationalLightDeflection = 1U << 2U,
    AnnualParallax = 1U << 3U,
    DiurnalParallax = 1U << 4U,
    PrecessionNutation = 1U << 5U,
    EarthOrientation = 1U << 6U,
    AtmosphericRefraction = 1U << 7U,
    ProperMotion = 1U << 8U,
    RadialVelocity = 1U << 9U,
    StellarParallax = 1U << 10U,
    Astrometric = (1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U)
        | (1U << 10U),
    Apparent = (1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U)
        | (1U << 6U),
    Topocentric = (1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U) | (1U << 10U)
        | (1U << 6U) | (1U << 4U),
    ApparentTopocentric = (1U << 0U) | (1U << 1U) | (1U << 2U) | (1U << 3U) | (1U << 5U) | (1U << 8U) | (1U << 9U)
        | (1U << 10U) | (1U << 6U) | (1U << 4U) | (1U << 7U)
};

}  // namespace skygate::ephemeris
