#pragma once

#include "skygate/core/Types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ephemeris {

enum class CelestialBodyType : std::uint8_t {
    Star,
    Planet,
    Moon,
    Sun,
    Constellation
};

struct CelestialBody {
    std::string id;
    std::string displayName;
    CelestialBodyType type = CelestialBodyType::Star;
    double visualMagnitude = 0.0;
    std::optional<core::EquatorialCoordinate> fixedEquatorial;
};

struct CelestialBodyState {
    CelestialBody body;
    core::EquatorialCoordinate equatorial;
    core::HorizontalCoordinate horizontal;
};

struct SkySnapshot {
    core::SkyContext context;
    std::vector<CelestialBodyState> states;
};

}  // namespace skygate::ephemeris
