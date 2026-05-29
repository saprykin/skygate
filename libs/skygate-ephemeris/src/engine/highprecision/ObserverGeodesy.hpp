#pragma once

#include "GeoLocation.hpp"
#include "HighPrecisionTypes.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

[[nodiscard]] std::optional<skygate::core::Vector3d>
observerItrsPositionAu(const skygate::core::GeoLocation& observer) noexcept;

}  // namespace skygate::ephemeris::highprecision
