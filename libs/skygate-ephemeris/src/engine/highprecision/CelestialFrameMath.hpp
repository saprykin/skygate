#pragma once

#include "EquatorialCoordinate.hpp"
#include "GeoLocation.hpp"
#include "HorizontalCoordinate.hpp"
#include "math/Vector3d.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

class CelestialFrameMath final {
public:
    [[nodiscard]] static skygate::core::Vector3d
    fromEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept;
    [[nodiscard]] static std::optional<skygate::core::EquatorialCoordinate>
    toEquatorial(const skygate::core::Vector3d& vector) noexcept;
    [[nodiscard]] static std::optional<skygate::core::HorizontalCoordinate> horizontalFromItrsVector(
        const skygate::core::Vector3d& vector, const skygate::core::GeoLocation& observer
    ) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
