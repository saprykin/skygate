#include "CatalogFieldOrigins.hpp"

#include <cmath>
#include <utility>

namespace skygate::ephemeris {

[[nodiscard]] bool CatalogFieldOrigins::outranks(const ValueOrigin& candidate, const ValueOrigin& current)
{
    if (candidate.sourceRank != current.sourceRank) {
        return candidate.sourceRank > current.sourceRank;
    }
    return candidate.rowOrdinal < current.rowOrdinal;
}

[[nodiscard]] CatalogFieldOrigins CatalogFieldOrigins::originsOf(
    const BaseCelestialBody& body, const std::size_t sourceRank, const std::size_t rowOrdinal
)
{
    const ValueOrigin origin{sourceRank, rowOrdinal};
    CatalogFieldOrigins origins;
    if (!body.displayName.empty()) {
        origins.displayName = origin;
    }
    if (!std::isnan(body.visualMagnitude)) {
        origins.visualMagnitude = origin;
    }
    if (body.fixedEquatorialValue().has_value()) {
        origins.fixedEquatorial = origin;
        origins.coordinateModel = origin;
    }

    const CatalogStarAstrometry* astrometry = body.catalogStarAstrometry();
    if (astrometry != nullptr) {
        origins.starAstrometry = origin;
        origins.coordinateModel = origin;
        if (astrometry->properMotionRightAscensionMasPerYear.has_value()) {
            origins.properMotionRightAscension = origin;
        }
        if (astrometry->properMotionDeclinationMasPerYear.has_value()) {
            origins.properMotionDeclination = origin;
        }
        if (astrometry->stellarParallaxMas.has_value()) {
            origins.stellarParallax = origin;
        }
        if (astrometry->radialVelocityKmPerSecond.has_value()) {
            origins.radialVelocity = origin;
        }
        if (astrometry->validityRange.has_value()) {
            origins.astrometryValidityRange = origin;
        }
    }

    const DeepSkyObjectInfo* deepSky = body.deepSkyObjectInfo();
    if (deepSky != nullptr) {
        if (deepSky->kind != DeepSkyObjectInfo::Kind::Unknown) {
            origins.deepSkyKind = origin;
        }
        if (deepSky->majorAxisArcmin.has_value()) {
            origins.majorAxis = origin;
        }
        if (deepSky->minorAxisArcmin.has_value()) {
            origins.minorAxis = origin;
        }
        if (deepSky->positionAngleDeg.has_value()) {
            origins.positionAngle = origin;
        }
    }
    return origins;
}

void CatalogFieldOrigins::adoptAstrometryOrigins(CatalogFieldOrigins& target, const CatalogFieldOrigins& source)
{
    target.starAstrometry = source.starAstrometry;
    target.properMotionRightAscension = source.properMotionRightAscension;
    target.properMotionDeclination = source.properMotionDeclination;
    target.stellarParallax = source.stellarParallax;
    target.radialVelocity = source.radialVelocity;
    target.astrometryValidityRange = source.astrometryValidityRange;
}

void CatalogFieldOrigins::clearAstrometryOrigins(CatalogFieldOrigins& target)
{
    target.starAstrometry = std::nullopt;
    target.properMotionRightAscension = std::nullopt;
    target.properMotionDeclination = std::nullopt;
    target.stellarParallax = std::nullopt;
    target.radialVelocity = std::nullopt;
    target.astrometryValidityRange = std::nullopt;
}

void CatalogFieldOrigins::adoptDeepSkyOrigins(CatalogFieldOrigins& target, const CatalogFieldOrigins& source)
{
    target.deepSkyKind = source.deepSkyKind;
    target.majorAxis = source.majorAxis;
    target.minorAxis = source.minorAxis;
    target.positionAngle = source.positionAngle;
}

void CatalogFieldOrigins::refreshCoordinateModelOrigin(CatalogFieldOrigins& origins)
{
    std::optional<ValueOrigin> model;
    if (origins.fixedEquatorial.has_value()) {
        model = origins.fixedEquatorial;
    }
    if (origins.starAstrometry.has_value() && (!model.has_value() || outranks(*origins.starAstrometry, *model))) {
        model = origins.starAstrometry;
    }
    origins.coordinateModel = std::move(model);
}

}  // namespace skygate::ephemeris
