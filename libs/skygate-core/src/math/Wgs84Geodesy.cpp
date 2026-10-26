#include "Wgs84Geodesy.hpp"
#include "AngleMath.hpp"
#include "Matrix3x3.hpp"
#include "PhysicalConstants.hpp"

#include <cassert>
#include <cmath>

namespace skygate::core {
namespace {

struct GeodeticTrigonometry final {
    double sinLatitude = 0.0;
    double cosLatitude = 0.0;
    double sinLongitude = 0.0;
    double cosLongitude = 0.0;
};

[[nodiscard]] GeodeticTrigonometry geodeticTrigonometry(const GeoLocation& location) noexcept
{
    const double latitudeRad = AngleMath::toRadians(location.latitudeDeg);
    const double longitudeRad = AngleMath::toRadians(location.longitudeDeg);
    return {
        .sinLatitude = std::sin(latitudeRad),
        .cosLatitude = std::cos(latitudeRad),
        .sinLongitude = std::sin(longitudeRad),
        .cosLongitude = std::cos(longitudeRad),
    };
}

}  // namespace

Vector3d Wgs84Geodesy::ecefMeters(const GeoLocation& location) noexcept
{
    assert(location.isValid());

    const GeodeticTrigonometry trigonometry = geodeticTrigonometry(location);
    const double firstEccentricitySquared =
        PhysicalConstants::kWgs84Flattening * (2.0 - PhysicalConstants::kWgs84Flattening);
    const double primeVerticalRadius =
        PhysicalConstants::kWgs84EquatorialRadiusMeters
        / std::sqrt(1.0 - firstEccentricitySquared * trigonometry.sinLatitude * trigonometry.sinLatitude);

    const double xMeters =
        (primeVerticalRadius + location.elevationMeters) * trigonometry.cosLatitude * trigonometry.cosLongitude;
    const double yMeters =
        (primeVerticalRadius + location.elevationMeters) * trigonometry.cosLatitude * trigonometry.sinLongitude;
    const double zMeters =
        (primeVerticalRadius * (1.0 - firstEccentricitySquared) + location.elevationMeters) * trigonometry.sinLatitude;

    return {
        .x = xMeters,
        .y = yMeters,
        .z = zMeters,
    };
}

Vector3d Wgs84Geodesy::eastNorthUpMeters(const GeoLocation& location, const Vector3d& ecefMeters) noexcept
{
    assert(location.isValid());

    const GeodeticTrigonometry trigonometry = geodeticTrigonometry(location);
    const Matrix3x3 localFrame{
        {.x = -trigonometry.sinLongitude, .y = trigonometry.cosLongitude, .z = 0.0},
        {.x = -trigonometry.sinLatitude * trigonometry.cosLongitude,
         .y = -trigonometry.sinLatitude * trigonometry.sinLongitude,
         .z = trigonometry.cosLatitude},
        {.x = trigonometry.cosLatitude * trigonometry.cosLongitude,
         .y = trigonometry.cosLatitude * trigonometry.sinLongitude,
         .z = trigonometry.sinLatitude},
    };
    return localFrame.multiplied(ecefMeters);
}

}  // namespace skygate::core
