#pragma once

#include "skygate/core/ProjectionTypes.hpp"

#include <array>

namespace skygate::core {

class SphericalGeometry final {
public:
    using Vector3d = std::array<double, 3>;

    [[nodiscard]] static double dot(const Vector3d& lhs, const Vector3d& rhs) noexcept;
    [[nodiscard]] static Vector3d cross(const Vector3d& lhs, const Vector3d& rhs) noexcept;
    [[nodiscard]] static double length(const Vector3d& vector) noexcept;
    [[nodiscard]] static Vector3d normalize(const Vector3d& vector) noexcept;

    [[nodiscard]] static Vector3d horizontalToUnitVector(
        const HorizontalCoordinate& coordinate
    ) noexcept;
    [[nodiscard]] static bool tryBuildProjectionBasis(
        const HorizontalCoordinate& centerCoordinate,
        Vector3d& center,
        Vector3d& right,
        Vector3d& up
    ) noexcept;
};

}  // namespace skygate::core
