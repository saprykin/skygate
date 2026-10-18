#pragma once

#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "Vector3d.hpp"

namespace skygate::core {

class SphericalGeometry final {
public:
    // Equatorial vectors point at right ascension 0h along x, right ascension
    // 6h along y, and the north celestial pole along z.
    [[nodiscard]] static Vector3d equatorialToUnitVector(const EquatorialCoordinate& coordinate) noexcept;
    [[nodiscard]] static EquatorialCoordinate equatorialFromUnitVector(const Vector3d& vector) noexcept;
    [[nodiscard]] static Vector3d horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept;
    [[nodiscard]] static HorizontalCoordinate horizontalFromUnitVector(const Vector3d& vector) noexcept;
    [[nodiscard]] static Vector3d
    rotatedAroundAxis(const Vector3d& vector, const Vector3d& axis, double angleRad) noexcept;
    // Returns the normalized axis that turns `from` toward `to`. Collinear
    // directions fall back to a perpendicular axis.
    [[nodiscard]] static Vector3d rotationAxisBetween(const Vector3d& from, const Vector3d& to) noexcept;
    // Returns the signed rotation about `axis` that takes `from` to `to`, in
    // radians. Collinear directions rotate by 0 or pi.
    [[nodiscard]] static double
    rotationAngleBetween(const Vector3d& from, const Vector3d& to, const Vector3d& axis) noexcept;
    [[nodiscard]] static double angularSeparationRad(const Vector3d& lhs, const Vector3d& rhs) noexcept;
    [[nodiscard]] static double
    angularSeparationDegrees(const HorizontalCoordinate& lhs, const HorizontalCoordinate& rhs) noexcept;
    [[nodiscard]] static HorizontalCoordinate interpolateHorizontalCoordinate(
        const HorizontalCoordinate& previous, const HorizontalCoordinate& next, double fraction
    ) noexcept;
    [[nodiscard]] static bool tryBuildProjectionBasis(
        const HorizontalCoordinate& centerCoordinate, Vector3d& center, Vector3d& right, Vector3d& up
    ) noexcept;
};

}  // namespace skygate::core
