#pragma once

#include "HorizontalCoordinate.hpp"
#include "Vector3d.hpp"

namespace skygate::core {

class SphericalGeometry final {
public:
    [[nodiscard]] static Vector3d horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept;
    [[nodiscard]] static HorizontalCoordinate horizontalFromUnitVector(const Vector3d& vector) noexcept;
    [[nodiscard]] static bool tryBuildProjectionBasis(
        const HorizontalCoordinate& centerCoordinate, Vector3d& center, Vector3d& right, Vector3d& up
    ) noexcept;
};

}  // namespace skygate::core
