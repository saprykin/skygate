#include "EclipticToEquatorialCalculator.hpp"
#include "math/AngleMath.hpp"
#include "math/SphericalGeometry.hpp"

namespace skygate::ephemeris {
namespace {

// The mean obliquity is a rotation about the equinox line, which is the x axis
// of both frames.
constexpr skygate::core::Vector3d kEquinoxAxis{.x = 1.0, .y = 0.0, .z = 0.0};

}  // namespace

skygate::core::EquatorialCoordinate EclipticToEquatorialCalculator::compute(
    const double eclipticLongitudeDeg, const double eclipticLatitudeDeg, const double obliquityDeg
) noexcept
{
    // The ecliptic frame shares its longitude and latitude convention with the
    // equatorial one, so the equatorial unit-vector helpers build the ecliptic
    // vector from the ecliptic angles as well.
    const skygate::core::Vector3d eclipticVector = skygate::core::SphericalGeometry::equatorialToUnitVector(
        {.rightAscensionHours = skygate::core::AngleMath::degreesToHours(eclipticLongitudeDeg),
         .declinationDeg = eclipticLatitudeDeg}
    );
    const skygate::core::Vector3d equatorialVector = skygate::core::SphericalGeometry::rotatedAroundAxis(
        eclipticVector, kEquinoxAxis, skygate::core::AngleMath::toRadians(obliquityDeg)
    );

    return skygate::core::SphericalGeometry::equatorialFromUnitVector(equatorialVector);
}

}  // namespace skygate::ephemeris
