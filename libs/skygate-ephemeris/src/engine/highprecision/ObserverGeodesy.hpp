#pragma once

#include "GeoLocation.hpp"
#include "math/Vector3d.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

class ObserverGeodesy final {
public:
    [[nodiscard]] static std::optional<skygate::core::Vector3d>
    observerItrsPositionAu(const skygate::core::GeoLocation& observer) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
