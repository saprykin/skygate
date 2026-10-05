#include "CatalogCoordinateModel.hpp"

#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"
#include "math/TimeConstants.hpp"

#include <cmath>

namespace skygate::ephemeris {
namespace {

// Positions that agree within one arcsecond describe the same direction.
// Independent catalogs place a matched object well inside this bound, so a
// larger separation is an unrelated coordinate model rather than noise.
constexpr double kDirectionToleranceArcseconds = 1.0;

constexpr double kDegreesPerHour = 15.0;

// Right-ascension difference projected onto the tangent plane at the mean
// declination, so it can be combined with the declination difference as one
// small-angle separation.
[[nodiscard]] double tangentPlaneRaDifferenceRadians(
    const skygate::core::EquatorialCoordinate& from, const skygate::core::EquatorialCoordinate& to
) noexcept
{
    const double deltaRaDeg = skygate::core::AngleMath::normalizeDegreesSigned(
        (to.rightAscensionHours - from.rightAscensionHours) * kDegreesPerHour
    );
    const double meanDeclinationRad =
        skygate::core::AngleMath::toRadians(0.5 * (from.declinationDeg + to.declinationDeg));
    return skygate::core::AngleMath::toRadians(deltaRaDeg) * std::cos(meanDeclinationRad);
}

[[nodiscard]] double declinationDifferenceRadians(
    const skygate::core::EquatorialCoordinate& from, const skygate::core::EquatorialCoordinate& to
) noexcept
{
    return skygate::core::AngleMath::toRadians(to.declinationDeg - from.declinationDeg);
}

[[nodiscard]] bool withinDirectionTolerance(const double raOffsetRadians, const double decOffsetRadians) noexcept
{
    return std::hypot(raOffsetRadians, decOffsetRadians)
           <= kDirectionToleranceArcseconds * skygate::core::MathConstants::kArcsecondsToRadians;
}

}  // namespace

bool CatalogCoordinateModel::sameDirection(
    const skygate::core::EquatorialCoordinate& lhs, const skygate::core::EquatorialCoordinate& rhs
) noexcept
{
    if (!lhs.isFinite() || !rhs.isFinite()) {
        return false;
    }

    return withinDirectionTolerance(tangentPlaneRaDifferenceRadians(lhs, rhs), declinationDifferenceRadians(lhs, rhs));
}

bool CatalogCoordinateModel::sameAstrometry(
    const CatalogStarAstrometry& winner, const CatalogStarAstrometry& loser
) noexcept
{
    // Only two declared reference epochs produce an epoch difference to
    // reconcile. A reference position with an undeclared epoch claims the same
    // epoch as the winning record, matching the snapshot validation rule.
    const double elapsedYears = winner.referenceEpoch.hasExplicit() && loser.referenceEpoch.hasExplicit()
                                    ? (winner.referenceEpoch.sortKey() - loser.referenceEpoch.sortKey())
                                          / skygate::core::TimeConstants::kJulianDaysPerYear
                                    : 0.0;
    const double raOffsetRadians =
        tangentPlaneRaDifferenceRadians(winner.referenceEquatorial, loser.referenceEquatorial)
        + loser.properMotionRightAscensionMasPerYear.value_or(0.0) * elapsedYears
              * skygate::core::MathConstants::kMilliarcsecondsToRadians;
    const double decOffsetRadians = declinationDifferenceRadians(winner.referenceEquatorial, loser.referenceEquatorial)
                                    + loser.properMotionDeclinationMasPerYear.value_or(0.0) * elapsedYears
                                          * skygate::core::MathConstants::kMilliarcsecondsToRadians;

    return withinDirectionTolerance(raOffsetRadians, decOffsetRadians);
}

}  // namespace skygate::ephemeris
