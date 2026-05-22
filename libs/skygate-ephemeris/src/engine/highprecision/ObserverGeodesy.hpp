#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "skygate/core/Types.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

[[nodiscard]] std::optional<SolarSystemKernelVector> observerItrsPositionAu(const core::GeoLocation& observer) noexcept;

}  // namespace skygate::ephemeris::highprecision
