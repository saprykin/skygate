#pragma once

#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/core/math/SphericalGeometry.hpp"

namespace skygate::core {

class ProjectionMath final {
public:
    using Vec3 = SphericalGeometry::Vector3d;

    [[nodiscard]] static bool isFinite(double value) noexcept;
    [[nodiscard]] static bool areValidProjectionParams(const ProjectionParams& params) noexcept;
    [[nodiscard]] static double toRadians(double degrees) noexcept;

    [[nodiscard]] static double dot(const Vec3& lhs, const Vec3& rhs) noexcept;
    [[nodiscard]] static Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept;
    [[nodiscard]] static double length(const Vec3& vector) noexcept;
    [[nodiscard]] static Vec3 normalize(const Vec3& vector) noexcept;

    [[nodiscard]] static Vec3 horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept;
    [[nodiscard]] static bool tryBuildProjectionBasis(
        const HorizontalCoordinate& centerCoordinate,
        Vec3& center,
        Vec3& right,
        Vec3& up
    ) noexcept;

    static void applyRoll(double& x, double& y, double rollDeg) noexcept;
};

}  // namespace skygate::core
