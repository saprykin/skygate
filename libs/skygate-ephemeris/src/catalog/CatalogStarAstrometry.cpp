#include "CatalogStarAstrometry.hpp"
#include "math/MathConstants.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

namespace skygate::ephemeris {

double CatalogStarAstrometry::parallaxMasFromDistanceParsecs(const double distanceParsecs) noexcept
{
    return skygate::core::MathConstants::kMilliarcsecondsPerArcsecond / distanceParsecs;
}

double CatalogStarAstrometry::distanceParsecsFromParallaxMas(const double parallaxMas) noexcept
{
    return skygate::core::MathConstants::kMilliarcsecondsPerArcsecond / parallaxMas;
}

bool CatalogStarAstrometry::hasUsableProperMotionRightAscension() const noexcept
{
    return properMotionRightAscensionMasPerYear.has_value()
           && CatalogSnapshotValidator::isKnownOptionalValue(properMotionRightAscensionMasPerYear);
}

bool CatalogStarAstrometry::hasUsableProperMotionDeclination() const noexcept
{
    return properMotionDeclinationMasPerYear.has_value()
           && CatalogSnapshotValidator::isKnownOptionalValue(properMotionDeclinationMasPerYear);
}

bool CatalogStarAstrometry::hasUsableParallax() const noexcept
{
    return stellarParallaxMas.has_value() && CatalogSnapshotValidator::isKnownPositiveOptionalValue(stellarParallaxMas);
}

bool CatalogStarAstrometry::hasUsableRadialVelocity() const noexcept
{
    return radialVelocityKmPerSecond.has_value()
           && CatalogSnapshotValidator::isKnownOptionalValue(radialVelocityKmPerSecond);
}

}  // namespace skygate::ephemeris
