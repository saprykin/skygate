#include "skygate/core/math/ProjectionMath.hpp"

#include "skygate/core/math/AngleMath.hpp"

#include <cmath>

namespace skygate::core {

bool ProjectionMath::isFinite(const double value) noexcept
{
    return std::isfinite(value);
}

bool ProjectionMath::areValidProjectionParams(const ProjectionParams& params) noexcept
{
    return params.isProjectable();
}

double ProjectionMath::toRadians(const double degrees) noexcept
{
    return AngleMath::toRadians(degrees);
}

double ProjectionMath::dot(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return SphericalGeometry::dot(lhs, rhs);
}

ProjectionMath::Vec3 ProjectionMath::cross(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return SphericalGeometry::cross(lhs, rhs);
}

double ProjectionMath::length(const Vec3& vector) noexcept
{
    return SphericalGeometry::length(vector);
}

ProjectionMath::Vec3 ProjectionMath::normalize(const Vec3& vector) noexcept
{
    return SphericalGeometry::normalize(vector);
}

ProjectionMath::Vec3 ProjectionMath::horizontalToUnitVector(
    const HorizontalCoordinate& coordinate
) noexcept
{
    return SphericalGeometry::horizontalToUnitVector(coordinate);
}

bool ProjectionMath::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate,
    Vec3& center,
    Vec3& right,
    Vec3& up
) noexcept
{
    return SphericalGeometry::tryBuildProjectionBasis(centerCoordinate, center, right, up);
}

void ProjectionMath::applyRoll(double& x, double& y, const double rollDeg) noexcept
{
    const double rollRad = toRadians(rollDeg);
    const double cosRoll = std::cos(rollRad);
    const double sinRoll = std::sin(rollRad);
    const double rotatedX = (x * cosRoll) - (y * sinRoll);
    const double rotatedY = (x * sinRoll) + (y * cosRoll);
    x = rotatedX;
    y = rotatedY;
}

}  // namespace skygate::core
