#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "EquatorialCoordinate.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "SkyRenderBuilders.hpp"
#include "trail/BodyTrailCalculator.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
class IEphemerisEngine;
}

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
};

class SkyObjectTrailBuilder final {
public:
    void appendTrail(SkyRenderFrame& frame, const SkyObjectTrailInput& input) const;

private:
    struct TrailSampleCacheKey final {
        const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
        skygate::core::ObservationContext context;
        std::optional<skygate::ephemeris::AstronomicalEpoch> requestEpoch;
        std::optional<skygate::ephemeris::EphemerisEngineOptions> requestOptions;
        std::optional<skygate::core::EquatorialCoordinate> targetEquatorial;
        std::uint32_t targetBodyIndex = 0;
    };

    [[nodiscard]] const std::vector<skygate::ephemeris::BodyTrailSample>& trailSamples(
        const SkyObjectTrailInput& input,
        const skygate::ephemeris::BodyTrailCalculator& trailCalculator,
        const skygate::ephemeris::BodyTrailOptions& trailOptions
    ) const;
    [[nodiscard]] static TrailSampleCacheKey sampleCacheKeyFor(const SkyObjectTrailInput& input);
    [[nodiscard]] static bool
    sampleCacheKeysEqual(const TrailSampleCacheKey& lhs, const TrailSampleCacheKey& rhs) noexcept;

    mutable std::optional<TrailSampleCacheKey> m_sampleCacheKey;
    mutable std::vector<skygate::ephemeris::BodyTrailSample> m_sampleCache;
};
