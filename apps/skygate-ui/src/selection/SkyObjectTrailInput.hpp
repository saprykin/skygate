#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequest.hpp"
#include "ObservationContext.hpp"
#include "PreparedProjection.hpp"
#include "SkyThemeRenderPalette.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace skygate::ephemeris {
class IEphemerisEngine;
class IEphemerisGuidanceStrategy;
}  // namespace skygate::ephemeris

struct SkyObjectTrailInput final {
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    const skygate::core::PreparedProjection* preparedProjection = nullptr;
    const skygate::ephemeris::BaseCelestialBody* targetBody = nullptr;
    const skygate::ephemeris::CelestialBodyState* targetState = nullptr;
    skygate::core::ObservationContext skyContext;
    std::optional<skygate::ephemeris::EphemerisRequest> ephemerisRequest;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    std::uint32_t targetBodyIndex = 0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    std::shared_ptr<skygate::ephemeris::IEphemerisGuidanceStrategy> guidanceStrategy;
};
