#include "CelestialFrameMath.hpp"
#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"
#include "math/SphericalGeometry.hpp"
#include "math/Wgs84Geodesy.hpp"

#include <cmath>
#include <limits>

namespace skygate::ephemeris::highprecision {

skygate::core::Vector3d
CelestialFrameMath::fromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    return skygate::core::SphericalGeometry::equatorialToUnitVector(coordinate);
}

skygate::core::Vector3d
CelestialFrameMath::eastBasisFromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * skygate::core::MathConstants::kRadiansPerHour;
    return {
        .x = -std::sin(rightAscensionRad),
        .y = std::cos(rightAscensionRad),
        .z = 0.0,
    };
}

skygate::core::Vector3d
CelestialFrameMath::northBasisFromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = coordinate.rightAscensionHours * skygate::core::MathConstants::kRadiansPerHour;
    const double declinationRad = skygate::core::AngleMath::toRadians(coordinate.declinationDeg);
    return {
        .x = -std::sin(declinationRad) * std::cos(rightAscensionRad),
        .y = -std::sin(declinationRad) * std::sin(rightAscensionRad),
        .z = std::cos(declinationRad),
    };
}

std::optional<skygate::core::EquatorialCoordinate>
CelestialFrameMath::toEquatorial(const skygate::core::Vector3d& vector) noexcept
{
    if (!vector.isFinite()) {
        return std::nullopt;
    }

    if (vector.length() <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    return skygate::core::SphericalGeometry::equatorialFromUnitVector(vector);
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

    const std::optional<skygate::core::Vector3d> localUnit =
        skygate::core::Wgs84Geodesy::eastNorthUpMeters(observer, vector).normalized();
    if (!localUnit.has_value()) {
        return std::nullopt;
    }

    return skygate::core::SphericalGeometry::horizontalFromUnitVector(*localUnit);
}

}  // namespace skygate::ephemeris::highprecision
