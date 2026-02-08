#include "skygate/core/math/ProjectionMath.hpp"

#include "skygate/core/math/MathConstants.hpp"

#include <cmath>

namespace skygate::core {
namespace {

constexpr std::size_t kXIndex = 0;
constexpr std::size_t kYIndex = 1;
constexpr std::size_t kZIndex = 2;

constexpr ProjectionMath::Vec3 kZenithAxis {0.0, 0.0, 1.0};
constexpr ProjectionMath::Vec3 kNorthAxis {0.0, 1.0, 0.0};

}  // namespace

bool ProjectionMath::isFinite(const double value) noexcept
{
    return std::isfinite(value);
}

bool ProjectionMath::areValidProjectionParams(const ProjectionParams& params) noexcept
{
    return isFinite(params.fovDeg) && isFinite(params.rollDeg)
        && isFinite(params.viewportWidth) && isFinite(params.viewportHeight)
        && params.viewportWidth > 0.0 && params.viewportHeight > 0.0
        && params.fovDeg > 0.0 && params.fovDeg < 179.0;
}

double ProjectionMath::toRadians(const double degrees) noexcept
{
    return degrees * MathConstants::kDegreesToRadians;
}

double ProjectionMath::dot(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return (lhs[kXIndex] * rhs[kXIndex]) + (lhs[kYIndex] * rhs[kYIndex]) + (lhs[kZIndex] * rhs[kZIndex]);
}

ProjectionMath::Vec3 ProjectionMath::cross(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return {
        (lhs[kYIndex] * rhs[kZIndex]) - (lhs[kZIndex] * rhs[kYIndex]),
        (lhs[kZIndex] * rhs[kXIndex]) - (lhs[kXIndex] * rhs[kZIndex]),
        (lhs[kXIndex] * rhs[kYIndex]) - (lhs[kYIndex] * rhs[kXIndex])
    };
}

double ProjectionMath::length(const Vec3& vector) noexcept
{
    return std::sqrt(dot(vector, vector));
}

ProjectionMath::Vec3 ProjectionMath::normalize(const Vec3& vector) noexcept
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

ProjectionMath::Vec3 ProjectionMath::horizontalToUnitVector(
    const HorizontalCoordinate& coordinate
) noexcept
{
    const double altitudeRad = toRadians(coordinate.altitudeDeg);
    const double azimuthRad = toRadians(coordinate.azimuthDeg);

    const double cosAltitude = std::cos(altitudeRad);
    return {
        cosAltitude * std::sin(azimuthRad),
        cosAltitude * std::cos(azimuthRad),
        std::sin(altitudeRad)
    };
}

bool ProjectionMath::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate,
    Vec3& center,
    Vec3& right,
    Vec3& up
) noexcept
{
    center = horizontalToUnitVector(centerCoordinate);

    right = cross(kZenithAxis, center);
    if (length(right) <= MathConstants::kEpsilon) {
        right = cross(kNorthAxis, center);
    }
    right = normalize(right);
    if (length(right) <= MathConstants::kEpsilon) {
        return false;
    }

    up = normalize(cross(center, right));
    return length(up) > MathConstants::kEpsilon;
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
