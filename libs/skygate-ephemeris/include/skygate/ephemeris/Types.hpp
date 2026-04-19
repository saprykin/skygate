#pragma once

#include "skygate/core/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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

enum class CelestialBodyEphemerisSource : std::uint8_t {
    Unresolved,
    FixedEquatorial,
    Sun,
    Moon,
    Planet,
    Star,
    Constellation
};

struct CelestialBody {
    std::string id;
    std::string displayName;
    CelestialBodyType type = CelestialBodyType::Star;
    CelestialBodyEphemerisSource ephemerisSource = CelestialBodyEphemerisSource::Unresolved;
    double visualMagnitude = 0.0;
    std::optional<core::EquatorialCoordinate> fixedEquatorial;
};

struct CelestialBodyState {
    std::uint32_t bodyIndex = 0;
    core::EquatorialCoordinate equatorial;
    core::HorizontalCoordinate horizontal;
};

struct SkySnapshot {
    core::SkyContext context;
    std::shared_ptr<const std::vector<CelestialBody>> catalogBodies;
    std::vector<CelestialBodyState> states;

    [[nodiscard]] std::span<const CelestialBody> bodies() const noexcept
    {
        if (catalogBodies == nullptr) {
            return {};
        }

        return std::span<const CelestialBody>(*catalogBodies);
    }

    [[nodiscard]] const CelestialBody& bodyAt(const std::size_t bodyIndex) const noexcept
    {
        return (*catalogBodies)[bodyIndex];
    }
};

}  // namespace skygate::ephemeris
