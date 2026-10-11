#pragma once

#include "EphemerisRequest.hpp"
#include "ObservationContext.hpp"

#include <cstdint>
#include <optional>

namespace skygate::ephemeris {
class BaseCelestialBody;
struct CelestialBodyState;
class IEphemerisEngine;
class IEphemerisGuidanceStrategy;
}  // namespace skygate::ephemeris

// Sampling inputs for one trail. They cover the engine, target, and
// observation context only; projection, theme, and viewport state stay
// with the builder.
struct SkyObjectTrailSamplingInput final {
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    const skygate::ephemeris::BaseCelestialBody* targetBody = nullptr;
    const skygate::ephemeris::CelestialBodyState* targetState = nullptr;
    skygate::core::ObservationContext skyContext;
    std::optional<skygate::ephemeris::EphemerisRequest> ephemerisRequest;
    std::uint32_t targetBodyIndex = 0;
    const skygate::ephemeris::IEphemerisGuidanceStrategy* guidanceStrategy = nullptr;
};
