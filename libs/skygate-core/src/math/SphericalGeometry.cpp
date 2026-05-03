#include "skygate/core/math/SphericalGeometry.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/MathConstants.hpp"

#include <cstddef>
#include <cmath>

namespace skygate::core {
namespace {

constexpr std::size_t kXIndex = 0;
constexpr std::size_t kYIndex = 1;
constexpr std::size_t kZIndex = 2;

}  // namespace

double SphericalGeometry::dot(const Vector3d& lhs, const Vector3d& rhs) noexcept
{
    return (lhs[kXIndex] * rhs[kXIndex])
        + (lhs[kYIndex] * rhs[kYIndex])
        + (lhs[kZIndex] * rhs[kZIndex]);
}

SphericalGeometry::Vector3d SphericalGeometry::cross(
    const Vector3d& lhs,
    const Vector3d& rhs
) noexcept
{
    return {
        (lhs[kYIndex] * rhs[kZIndex]) - (lhs[kZIndex] * rhs[kYIndex]),
        (lhs[kZIndex] * rhs[kXIndex]) - (lhs[kXIndex] * rhs[kZIndex]),
        (lhs[kXIndex] * rhs[kYIndex]) - (lhs[kYIndex] * rhs[kXIndex])
    };
}

double SphericalGeometry::length(const Vector3d& vector) noexcept
{
    return std::sqrt(dot(vector, vector));
}

SphericalGeometry::Vector3d SphericalGeometry::normalize(const Vector3d& vector) noexcept
{
    const double vectorLength = length(vector);
    if (vectorLength <= MathConstants::kEpsilon) {
        return {};
    }

    return {
        vector[kXIndex] / vectorLength,
        vector[kYIndex] / vectorLength,
        vector[kZIndex] / vectorLength
    };
}

SphericalGeometry::Vector3d SphericalGeometry::horizontalToUnitVector(
    const HorizontalCoordinate& coordinate
) noexcept
{
    const double altitudeRad = AngleMath::toRadians(coordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(coordinate.azimuthDeg);

    const double cosAltitude = std::cos(altitudeRad);
    return {
        cosAltitude * std::sin(azimuthRad),
        cosAltitude * std::cos(azimuthRad),
        std::sin(altitudeRad)
    };
}

bool SphericalGeometry::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate,
    Vector3d& center,
    Vector3d& right,
    Vector3d& up
) noexcept
{
    if (!centerCoordinate.isValid()) {
        return false;
    }

    const HorizontalCoordinate normalizedCoordinate = centerCoordinate.normalizedAzimuth();
    const double altitudeRad = AngleMath::toRadians(normalizedCoordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(normalizedCoordinate.azimuthDeg);

    const double sinAltitude = std::sin(altitudeRad);
    const double cosAltitude = std::cos(altitudeRad);
    const double sinAzimuth = std::sin(azimuthRad);
    const double cosAzimuth = std::cos(azimuthRad);

    center = {
        cosAltitude * sinAzimuth,
        cosAltitude * cosAzimuth,
        sinAltitude
    };
    right = {
        -cosAzimuth,
        sinAzimuth,
        0.0
    };
    up = {
        -sinAltitude * sinAzimuth,
        -sinAltitude * cosAzimuth,
        cosAltitude
    };
    return true;
}

}  // namespace skygate::core
