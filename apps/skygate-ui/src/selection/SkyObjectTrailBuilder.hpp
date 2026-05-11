#pragma once

#include "SkyRenderBuilders.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>

namespace skygate::ephemeris {
class IEphemerisEngine;
}

struct SkyObjectTrailInput final {
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    const skygate::core::PreparedProjection* preparedProjection = nullptr;
    skygate::core::SkyContext skyContext;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    std::uint32_t targetBodyIndex = 0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

class SkyObjectTrailBuilder final {
public:
    void appendTrail(SkyRenderFrame& frame, const SkyObjectTrailInput& input) const;
};
