#pragma once

#include "EquatorialCoordinate.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris {

struct CatalogStarAstrometry {
    skygate::core::EquatorialCoordinate referenceEquatorial;
    skygate::core::AstronomicalEpoch referenceEpoch;
    // Tangent-plane RA proper motion, mu_alpha * cos(delta), in mas/year.
    std::optional<double> properMotionRightAscensionMasPerYear;
    std::optional<double> properMotionDeclinationMasPerYear;
    std::optional<double> stellarParallaxMas;
    std::optional<double> radialVelocityKmPerSecond;
    std::optional<EphemerisDateRange> validityRange;

    [[nodiscard]] bool hasUsableProperMotionRightAscension() const noexcept;
    [[nodiscard]] bool hasUsableProperMotionDeclination() const noexcept;
    [[nodiscard]] bool hasUsableParallax() const noexcept;
    [[nodiscard]] bool hasUsableRadialVelocity() const noexcept;
};

}  // namespace skygate::ephemeris
