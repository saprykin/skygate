#pragma once

#include "EquatorialCoordinate.hpp"
#include "ObservationContext.hpp"
#include "SkyObjectTrailSamplingInput.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "trail/BodyTrailSample.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace skygate::ephemeris {
class IEphemerisEngine;
class IEphemerisGuidanceStrategy;
}  // namespace skygate::ephemeris

// Chooses how a trail is sampled through the ephemeris engine and caches the
// result. The sample window covers the whole past/future range, so projection,
// theme, and viewport changes reuse the cache.
class SkyObjectTrailSampler final {
public:
    [[nodiscard]] const std::vector<skygate::ephemeris::BodyTrailSample>&
    samples(const SkyObjectTrailSamplingInput& input);
    [[nodiscard]] bool hasCachedSamples(const SkyObjectTrailSamplingInput& input) const;

    [[nodiscard]] static bool usesAdaptiveTrailSampling(const SkyObjectTrailSamplingInput& input) noexcept;
    [[nodiscard]] static bool shouldUseGuidanceTrail(const SkyObjectTrailSamplingInput& input) noexcept;
    [[nodiscard]] static bool isFixedEquatorialTrailTarget(const SkyObjectTrailSamplingInput& input) noexcept;

private:
    struct TrailSampleCacheKey final {
        const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
        const skygate::ephemeris::IEphemerisGuidanceStrategy* guidanceStrategy = nullptr;
        skygate::core::ObservationContext context;
        std::optional<skygate::core::AstronomicalEpoch> requestEpoch;
        std::optional<skygate::ephemeris::EphemerisEngineOptions> requestOptions;
        std::optional<skygate::core::EquatorialCoordinate> targetEquatorial;
        std::uint32_t targetBodyIndex = 0;
    };

    [[nodiscard]] static TrailSampleCacheKey sampleCacheKeyFor(const SkyObjectTrailSamplingInput& input);
    [[nodiscard]] static bool
    sampleCacheKeysEqual(const TrailSampleCacheKey& lhs, const TrailSampleCacheKey& rhs) noexcept;

    std::optional<TrailSampleCacheKey> m_sampleCacheKey;
    std::vector<skygate::ephemeris::BodyTrailSample> m_sampleCache;
};
