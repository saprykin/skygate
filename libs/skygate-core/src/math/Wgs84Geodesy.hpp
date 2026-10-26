#pragma once

#include "GeoLocation.hpp"
#include "Vector3d.hpp"

namespace skygate::core {

class Wgs84Geodesy final {
public:
    // Requires a valid location; returns the Earth-centered Earth-fixed
    // position in meters.
    [[nodiscard]] static Vector3d ecefMeters(const GeoLocation& location) noexcept;
    // Rotates an Earth-centered Earth-fixed vector into the local east, north,
    // and up axes in meters. Requires a valid location.
    [[nodiscard]] static Vector3d eastNorthUpMeters(const GeoLocation& location, const Vector3d& ecefMeters) noexcept;
};

}  // namespace skygate::core
