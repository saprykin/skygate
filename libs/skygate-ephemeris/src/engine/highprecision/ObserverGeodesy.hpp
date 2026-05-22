#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "skygate/core/GeoLocation.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

[[nodiscard]] std::optional<SolarSystemKernelVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept;

}  // namespace skygate::ephemeris::highprecision
