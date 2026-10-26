#pragma once

#include "EquatorialCoordinate.hpp"
#include "HorizontalCoordinate.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"

#include <cstdint>

namespace skygate::ephemeris {

struct CelestialBodyState {
    std::uint32_t bodyIndex = 0;
    skygate::core::EquatorialCoordinate equatorial;
    skygate::core::HorizontalCoordinate horizontal;
    EphemerisEngineQueryResult metadata;
};

}  // namespace skygate::ephemeris
