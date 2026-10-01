#pragma once

#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Vector3d.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

struct HighPrecisionCalculatorResult {
    std::optional<skygate::core::EquatorialCoordinate> equatorial;
    std::optional<skygate::core::HorizontalCoordinate> horizontal;
    std::optional<skygate::core::Vector3d> observerRelativePositionAu;
    EphemerisEngineQueryResult metadata;
};

}  // namespace skygate::ephemeris::highprecision
