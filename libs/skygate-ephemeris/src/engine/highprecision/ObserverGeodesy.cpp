#include "ObserverGeodesy.hpp"

#include "math/AngleMath.hpp"
#include "math/PhysicalConstants.hpp"

#include <cmath>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::PhysicalConstants;

}  // namespace

std::optional<skygate::core::Vector3d>
ObserverGeodesy::observerItrsPositionAu(const skygate::core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    const double latitudeRad = skygate::core::AngleMath::toRadians(observer.latitudeDeg);
    const double longitudeRad = skygate::core::AngleMath::toRadians(observer.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);
    const double firstEccentricitySquared =
        PhysicalConstants::kWgs84Flattening * (2.0 - PhysicalConstants::kWgs84Flattening);
    const double primeVerticalRadius = PhysicalConstants::kWgs84EquatorialRadiusMeters
                                       / std::sqrt(1.0 - firstEccentricitySquared * sinLatitude * sinLatitude);

    const double xMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * cosLongitude;
    const double yMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + observer.elevationMeters) * sinLatitude;

    return skygate::core::Vector3d{
        .x = xMeters / PhysicalConstants::kAstronomicalUnitMeters,
        .y = yMeters / PhysicalConstants::kAstronomicalUnitMeters,
        .z = zMeters / PhysicalConstants::kAstronomicalUnitMeters,
    };
}

}  // namespace skygate::ephemeris::highprecision
