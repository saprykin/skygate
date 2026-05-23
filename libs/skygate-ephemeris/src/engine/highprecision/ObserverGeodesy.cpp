#include "engine/highprecision/ObserverGeodesy.hpp"

#include "math/AngleMath.hpp"
#include "math/PhysicalConstants.hpp"

#include <cmath>

namespace skygate::ephemeris::highprecision {
namespace {

using core::PhysicalConstants;
constexpr double kWgs84EquatorialRadiusMeters = 6'378'137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;

}  // namespace

std::optional<SolarSystemKernelVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    const double latitudeRad = core::AngleMath::toRadians(observer.latitudeDeg);
    const double longitudeRad = core::AngleMath::toRadians(observer.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);
    const double firstEccentricitySquared = kWgs84Flattening * (2.0 - kWgs84Flattening);
    const double primeVerticalRadius =
        kWgs84EquatorialRadiusMeters / std::sqrt(1.0 - firstEccentricitySquared * sinLatitude * sinLatitude);

    const double xMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * cosLongitude;
    const double yMeters = (primeVerticalRadius + observer.elevationMeters) * cosLatitude * sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + observer.elevationMeters) * sinLatitude;

    return SolarSystemKernelVector{
        .xAu = xMeters / PhysicalConstants::kAstronomicalUnitMeters,
        .yAu = yMeters / PhysicalConstants::kAstronomicalUnitMeters,
        .zAu = zMeters / PhysicalConstants::kAstronomicalUnitMeters,
    };
}

}  // namespace skygate::ephemeris::highprecision
