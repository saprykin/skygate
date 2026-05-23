#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"
#include "GeoLocation.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

[[nodiscard]] std::optional<SolarSystemKernelVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept;

}  // namespace skygate::ephemeris::highprecision
