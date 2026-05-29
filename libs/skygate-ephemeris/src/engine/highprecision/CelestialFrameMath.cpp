#include "CelestialFrameMath.hpp"
#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace skygate::ephemeris::highprecision {

skygate::core::Vector3d
CelestialFrameMath::fromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * skygate::core::MathConstants::kRadiansPerHour;
    const double declinationRad = skygate::core::AngleMath::toRadians(coordinate.declinationDeg);
    const double cosDeclination = std::cos(declinationRad);
    return {
        .x = cosDeclination * std::cos(rightAscensionRad),
        .y = cosDeclination * std::sin(rightAscensionRad),
        .z = std::sin(declinationRad),
    };
}

std::optional<skygate::core::EquatorialCoordinate>
CelestialFrameMath::toEquatorial(const skygate::core::Vector3d& vector) noexcept
{
    if (!vector.isFinite()) {
        return std::nullopt;
    }

    const double xyDistance = std::hypot(vector.x, vector.y);
    const double distance = vector.length();
    if (distance <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    double rightAscensionHours = std::atan2(vector.y, vector.x) * skygate::core::MathConstants::kHoursPerRadian;
    if (rightAscensionHours < 0.0) {
        rightAscensionHours += 24.0;
    }

    return skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = skygate::core::AngleMath::toDegrees(std::atan2(vector.z, xyDistance)),
    };
}

std::optional<skygate::core::HorizontalCoordinate> CelestialFrameMath::horizontalFromItrsVector(
    const skygate::core::Vector3d& vector, const skygate::core::GeoLocation& observer
) noexcept
{
    if (!observer.isValid()) {
        return std::nullopt;
    }

    if (!vector.isFinite()) {
        return std::nullopt;
    }

    const double latitudeRad = skygate::core::AngleMath::toRadians(observer.latitudeDeg);
    const double longitudeRad = skygate::core::AngleMath::toRadians(observer.longitudeDeg);
    const double sinLatitude = std::sin(latitudeRad);
    const double cosLatitude = std::cos(latitudeRad);
    const double sinLongitude = std::sin(longitudeRad);
    const double cosLongitude = std::cos(longitudeRad);

    const double east = -sinLongitude * vector.x + cosLongitude * vector.y;
    const double north =
        -sinLatitude * cosLongitude * vector.x - sinLatitude * sinLongitude * vector.y + cosLatitude * vector.z;
    const double up =
        cosLatitude * cosLongitude * vector.x + cosLatitude * sinLongitude * vector.y + sinLatitude * vector.z;
    const double length = std::hypot(std::hypot(east, north), up);
    if (length <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    return skygate::core::HorizontalCoordinate{
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(up / length, -1.0, 1.0))),
        .azimuthDeg =
            skygate::core::AngleMath::normalizeDegrees(skygate::core::AngleMath::toDegrees(std::atan2(east, north))),
    };
}

}  // namespace skygate::ephemeris::highprecision
