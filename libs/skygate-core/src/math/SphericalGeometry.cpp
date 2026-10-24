#include "SphericalGeometry.hpp"
#include "AngleMath.hpp"
#include "MathConstants.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {
namespace {

struct HorizontalTrigonometry final {
    double sinAltitude = 0.0;
    double cosAltitude = 0.0;
    double sinAzimuth = 0.0;
    double cosAzimuth = 0.0;
};

// Direction used when a rotated or interpolated vector collapses to zero
// length, and as the polar reference that resolves a rotation axis for
// collinear inputs.
constexpr Vector3d kPoleDirection{.x = 0.0, .y = 0.0, .z = 1.0};
constexpr Vector3d kEquinoxDirection{.x = 1.0, .y = 0.0, .z = 0.0};

// For two unit vectors the cross-product length is the sine of the angle
// between them. Below this value the rotation axis is unresolved and the
// directions are treated as collinear.
constexpr double kCollinearSinTolerance = 1e-9;

[[nodiscard]] HorizontalTrigonometry horizontalTrigonometry(const HorizontalCoordinate& coordinate) noexcept
{
    const double altitudeRad = AngleMath::toRadians(coordinate.altitudeDeg);
    const double azimuthRad = AngleMath::toRadians(coordinate.azimuthDeg);
    return {
        .sinAltitude = std::sin(altitudeRad),
        .cosAltitude = std::cos(altitudeRad),
        .sinAzimuth = std::sin(azimuthRad),
        .cosAzimuth = std::cos(azimuthRad),
    };
}

[[nodiscard]] Vector3d unitVectorFromTrigonometry(const HorizontalTrigonometry& trigonometry) noexcept
{
    return {
        .x = trigonometry.cosAltitude * trigonometry.sinAzimuth,
        .y = trigonometry.cosAltitude * trigonometry.cosAzimuth,
        .z = trigonometry.sinAltitude,
    };
}

[[nodiscard]] Vector3d normalizedOrPole(const Vector3d& vector) noexcept
{
    return vector.normalized().value_or(kPoleDirection);
}

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
    return unitVectorFromTrigonometry(horizontalTrigonometry(coordinate));
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
    return normalizedOrPole(rotated);
}

Vector3d SphericalGeometry::rotationAxisBetween(const Vector3d& from, const Vector3d& to) noexcept
{
    Vector3d axis = from.cross(to);
    if (axis.length() <= kCollinearSinTolerance) {
        axis = from.cross(kPoleDirection);
        if (axis.length() <= kCollinearSinTolerance) {
            axis = from.cross(kEquinoxDirection);
        }
    }
    return normalizedOrPole(axis);
}

double SphericalGeometry::rotationAngleBetween(const Vector3d& from, const Vector3d& to, const Vector3d& axis) noexcept
{
    const double cosAngle = from.dot(to);
    const double sinAngle = from.cross(to).dot(axis);
    if (std::abs(sinAngle) <= kCollinearSinTolerance) {
        // Collinear directions leave the sine component unresolved, so the dot
        // product alone decides between no turn and a half turn.
        return cosAngle > 0.0 ? 0.0 : MathConstants::kPi;
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

Vector3d SphericalGeometry::interpolateUnitVector(
    const Vector3d& start, const Vector3d& end, const double angularDistanceRad, const double fraction
) noexcept
{
    const double sinAngularDistance = std::sin(angularDistanceRad);
    if (!std::isfinite(sinAngularDistance) || std::abs(sinAngularDistance) <= MathConstants::kEpsilon) {
        // The sine weights degenerate for collinear directions, so the chord
        // between the vectors is the only usable interpolation.
        const Vector3d chord = start + ((end - start) * fraction);
        return normalizedOrPole(chord);
    }

    const double startWeight = std::sin((1.0 - fraction) * angularDistanceRad) / sinAngularDistance;
    const double endWeight = std::sin(fraction * angularDistanceRad) / sinAngularDistance;
    const Vector3d interpolated = (start * startWeight) + (end * endWeight);
    return normalizedOrPole(interpolated);
}

HorizontalCoordinate SphericalGeometry::interpolateHorizontalCoordinate(
    const HorizontalCoordinate& previous, const HorizontalCoordinate& next, const double fraction
) noexcept
{
    const Vector3d previousVector = horizontalToUnitVector(previous);
    const Vector3d nextVector = horizontalToUnitVector(next);
    const double angularDistance = angularSeparationRad(previousVector, nextVector);
    return horizontalFromUnitVector(interpolateUnitVector(previousVector, nextVector, angularDistance, fraction));
}

bool SphericalGeometry::tryBuildProjectionBasis(
    const HorizontalCoordinate& centerCoordinate, Vector3d& center, Vector3d& right, Vector3d& up
) noexcept
{
    if (!centerCoordinate.isValid()) {
        return false;
    }

    const HorizontalCoordinate normalizedCoordinate = centerCoordinate.normalizedAzimuth();
    const HorizontalTrigonometry trigonometry = horizontalTrigonometry(normalizedCoordinate);

    center = unitVectorFromTrigonometry(trigonometry);
    right = {.x = -trigonometry.cosAzimuth, .y = trigonometry.sinAzimuth, .z = 0.0};
    up = {
        .x = -trigonometry.sinAltitude * trigonometry.sinAzimuth,
        .y = -trigonometry.sinAltitude * trigonometry.cosAzimuth,
        .z = trigonometry.cosAltitude,
    };
    return true;
}

}  // namespace skygate::core
