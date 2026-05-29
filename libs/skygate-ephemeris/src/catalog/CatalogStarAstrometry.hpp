#pragma once

#include "EquatorialCoordinate.hpp"
#include "engine/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris {

struct CatalogStarAstrometry {
    skygate::core::EquatorialCoordinate referenceEquatorial;
    AstronomicalEpoch referenceEpoch;
    // Tangent-plane RA proper motion, mu_alpha * cos(delta), in mas/year.
    std::optional<double> properMotionRightAscensionMasPerYear;
    std::optional<double> properMotionDeclinationMasPerYear;
    std::optional<double> stellarParallaxMas;
    std::optional<double> radialVelocityKmPerSecond;
    std::optional<EphemerisDateRange> validityRange;
};

}  // namespace skygate::ephemeris
