#include "Wgs84Geodesy.hpp"
#include "AngleMath.hpp"
#include "PhysicalConstants.hpp"

#include <cassert>
#include <cmath>

namespace skygate::core {

Vector3d Wgs84Geodesy::ecefMeters(const GeoLocation& location) noexcept
{
    assert(location.isValid());

    const double latitudeRad = AngleMath::toRadians(location.latitudeDeg);
    const double longitudeRad = AngleMath::toRadians(location.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);
    const double firstEccentricitySquared =
        PhysicalConstants::kWgs84Flattening * (2.0 - PhysicalConstants::kWgs84Flattening);
    const double primeVerticalRadius = PhysicalConstants::kWgs84EquatorialRadiusMeters
                                       / std::sqrt(1.0 - firstEccentricitySquared * sinLatitude * sinLatitude);

    const double xMeters = (primeVerticalRadius + location.elevationMeters) * cosLatitude * cosLongitude;
    const double yMeters = (primeVerticalRadius + location.elevationMeters) * cosLatitude * sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + location.elevationMeters) * sinLatitude;

    return {
        .x = xMeters,
        .y = yMeters,
        .z = zMeters,
    };
}

}  // namespace skygate::core
