#pragma once

#include "GeoLocation.hpp"
#include "Vector3d.hpp"

namespace skygate::core {

class Wgs84Geodesy final {
public:
    // Requires a valid location; returns the Earth-centered Earth-fixed
    // position in meters.
    [[nodiscard]] static Vector3d ecefMeters(const GeoLocation& location) noexcept;
};

}  // namespace skygate::core
