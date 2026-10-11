#include "CatalogStarAstrometry.hpp"

#include <cmath>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] bool hasFiniteValue(const std::optional<double>& value) noexcept
{
    return value.has_value() && std::isfinite(*value);
}

}  // namespace

bool CatalogStarAstrometry::hasUsableProperMotionRightAscension() const noexcept
{
    return hasFiniteValue(properMotionRightAscensionMasPerYear);
}

bool CatalogStarAstrometry::hasUsableProperMotionDeclination() const noexcept
{
    return hasFiniteValue(properMotionDeclinationMasPerYear);
}

bool CatalogStarAstrometry::hasUsableParallax() const noexcept
{
    return hasFiniteValue(stellarParallaxMas) && *stellarParallaxMas > 0.0;
}

bool CatalogStarAstrometry::hasUsableRadialVelocity() const noexcept
{
    return hasFiniteValue(radialVelocityKmPerSecond);
}

}  // namespace skygate::ephemeris
