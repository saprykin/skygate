#include "SphericalGeometry.hpp"
#include "AngleMath.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace skygate::core {
namespace {

// Direction used when a rotated or interpolated vector collapses to zero length.
constexpr Vector3d kZenithFallbackDirection{.x = 0.0, .y = 0.0, .z = 1.0};

// Reference directions that resolve a rotation axis for collinear inputs, tried
// in this order.
constexpr Vector3d kPolarReferenceDirection{.x = 0.0, .y = 0.0, .z = 1.0};
constexpr Vector3d kEquinoxReferenceDirection{.x = 1.0, .y = 0.0, .z = 0.0};

// For two unit vectors the cross-product length is the sine of the angle
// between them. Below this value the rotation axis is unresolved and the
// directions are treated as collinear.
constexpr double kCollinearSinTolerance = 1e-9;

}  // namespace

Vector3d SphericalGeometry::equatorialToUnitVector(const EquatorialCoordinate& coordinate) noexcept
{
    const double rightAscensionRad = AngleMath::hoursToRadians(coordinate.rightAscensionHours);
    const double declinationRad = AngleMath::toRadians(coordinate.declinationDeg);
    const double cosDeclination = std::cos(declinationRad);
    return {
        .x = cosDeclination * std::cos(rightAscensionRad),
        .y = cosDeclination * std::sin(rightAscensionRad),
        .z = std::sin(declinationRad),
    };
}

EquatorialCoordinate SphericalGeometry::equatorialFromUnitVector(const Vector3d& vector) noexcept
{
    return {
        .rightAscensionHours = AngleMath::normalizeHours(AngleMath::radiansToHours(std::atan2(vector.y, vector.x))),
        .declinationDeg = AngleMath::toDegrees(std::atan2(vector.z, std::hypot(vector.x, vector.y))),
    };
}

Vector3d SphericalGeometry::horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept
{
    const double altitudeRad = AngleMath::toRadians(coordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(coordinate.azimuthDeg);

    const double cosAltitude = std::cos(altitudeRad);
    return {
        .x = cosAltitude * std::sin(azimuthRad),
        .y = cosAltitude * std::cos(azimuthRad),
        .z = std::sin(altitudeRad),
    };
}

HorizontalCoordinate SphericalGeometry::horizontalFromUnitVector(const Vector3d& vector) noexcept
{
    return {
        .altitudeDeg = AngleMath::toDegrees(std::asin(std::clamp(vector.z, -1.0, 1.0))),
        .azimuthDeg = AngleMath::normalizeDegrees(AngleMath::toDegrees(std::atan2(vector.x, vector.y))),
    };
}

Vector3d
SphericalGeometry::rotatedAroundAxis(const Vector3d& vector, const Vector3d& axis, const double angleRad) noexcept
{
    const double cosAngle = std::cos(angleRad);
    const double sinAngle = std::sin(angleRad);
    const Vector3d axisCrossVector = axis.cross(vector);
    const double axisDotVector = axis.dot(vector);
    const Vector3d rotated =
        (vector * cosAngle) + (axisCrossVector * sinAngle) + (axis * (axisDotVector * (1.0 - cosAngle)));
    return rotated.normalized().value_or(kZenithFallbackDirection);
}

Vector3d SphericalGeometry::rotationAxisBetween(const Vector3d& from, const Vector3d& to) noexcept
{
    Vector3d axis = from.cross(to);
    if (axis.length() <= kCollinearSinTolerance) {
        axis = from.cross(kPolarReferenceDirection);
        if (axis.length() <= kCollinearSinTolerance) {
            axis = from.cross(kEquinoxReferenceDirection);
        }
    }
    return axis.normalized().value_or(kZenithFallbackDirection);
}

double SphericalGeometry::rotationAngleBetween(const Vector3d& from, const Vector3d& to, const Vector3d& axis) noexcept
{
    const double cosAngle = from.dot(to);
    const double sinAngle = from.cross(to).dot(axis);
    if (std::abs(sinAngle) <= kCollinearSinTolerance) {
        // Collinear directions leave the sine component unresolved, so the dot
        // product alone decides between no turn and a half turn.
        return cosAngle > 0.0 ? 0.0 : std::numbers::pi;
    }
    return std::atan2(sinAngle, cosAngle);
}

double SphericalGeometry::angularSeparationRad(const Vector3d& lhs, const Vector3d& rhs) noexcept
{
    return std::acos(std::clamp(lhs.dot(rhs), -1.0, 1.0));
}

double
SphericalGeometry::angularSeparationDegrees(const HorizontalCoordinate& lhs, const HorizontalCoordinate& rhs) noexcept
{
    const Vector3d lhsVector = horizontalToUnitVector(lhs.normalizedAzimuth());
    const Vector3d rhsVector = horizontalToUnitVector(rhs.normalizedAzimuth());
    return AngleMath::toDegrees(angularSeparationRad(lhsVector, rhsVector));
}

HorizontalCoordinate SphericalGeometry::interpolateHorizontalCoordinate(
    const HorizontalCoordinate& previous, const HorizontalCoordinate& next, const double fraction
) noexcept
{
    // Interpolates along the chord between the unit vectors and renormalizes.
    const Vector3d previousVector = horizontalToUnitVector(previous);
    const Vector3d nextVector = horizontalToUnitVector(next);
    const Vector3d interpolated = previousVector + ((nextVector - previousVector) * fraction);
    return horizontalFromUnitVector(interpolated.normalized().value_or(kZenithFallbackDirection));
}

bool SphericalGeometry::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate, Vector3d& center, Vector3d& right, Vector3d& up
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

    center = horizontalToUnitVector(normalizedCoordinate);
    right = {.x = -cosAzimuth, .y = sinAzimuth, .z = 0.0};
    up = {.x = -sinAltitude * sinAzimuth, .y = -sinAltitude * cosAzimuth, .z = cosAltitude};
    return true;
}

}  // namespace skygate::core
