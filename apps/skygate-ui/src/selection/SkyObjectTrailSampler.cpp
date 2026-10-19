#include "SkyObjectTrailSampler.hpp"
#include "CelestialBodyState.hpp"
#include "SkyObjectTrailAlignment.hpp"
#include "SkyObjectTrailInterpolator.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "trail/BodyTrailCalculator.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int kObjectTrailPastHours = 6;
constexpr int kObjectTrailFutureHours = 18;
constexpr int kObjectTrailSampleStepMinutes = 30;
constexpr int kObjectTrailAdaptiveRenderStepMinutes = 10;

[[nodiscard]] bool optionalEquatorialsEqual(
    const std::optional<skygate::core::EquatorialCoordinate>& lhs,
    const std::optional<skygate::core::EquatorialCoordinate>& rhs
) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || lhs->equals(*rhs);
}

[[nodiscard]] std::vector<skygate::ephemeris::BodyTrailSample>
sampleTrail(const SkyObjectTrailSamplingInput& input, const skygate::ephemeris::BodyTrailCalculator& trailCalculator)
{
    const skygate::ephemeris::BodyTrailOptions renderOptions{
        .pastHours = kObjectTrailPastHours,
        .futureHours = kObjectTrailFutureHours,
        .sampleStepMinutes = SkyObjectTrailSampler::usesAdaptiveTrailSampling(input)
                                 ? kObjectTrailAdaptiveRenderStepMinutes
                                 : kObjectTrailSampleStepMinutes
    };

    if (SkyObjectTrailSampler::isFixedEquatorialTrailTarget(input)) {
        std::vector<skygate::ephemeris::BodyTrailSample> samples =
            SkyObjectTrailInterpolator::sampleEngineWindow(input, renderOptions);
        SkyObjectTrailAlignment::alignToSelectedState(samples, input);
        return samples;
    }

    if (SkyObjectTrailSampler::shouldUseGuidanceTrail(input)) {
        std::optional<std::vector<skygate::ephemeris::BodyTrailSample>> guidanceSamples =
            SkyObjectTrailInterpolator::sampleGuidanceTrail(input, trailCalculator, renderOptions);
        if (guidanceSamples.has_value()) {
            SkyObjectTrailAlignment::alignToSelectedState(*guidanceSamples, input);
            return std::move(*guidanceSamples);
        }
    }

    if (SkyObjectTrailSampler::usesAdaptiveTrailSampling(input)) {
        return SkyObjectTrailInterpolator::sampleAdaptiveTrail(input, trailCalculator, renderOptions);
    }

    return input.ephemerisRequest.has_value()
               ? trailCalculator.sample(
                     *input.ephemerisEngine, *input.ephemerisRequest, input.targetBodyIndex, renderOptions
                 )
               : trailCalculator.sample(*input.ephemerisEngine, input.skyContext, input.targetBodyIndex, renderOptions);
}

}  // namespace

bool SkyObjectTrailSampler::usesAdaptiveTrailSampling(const SkyObjectTrailSamplingInput& input) noexcept
{
    return input.ephemerisEngine != nullptr && input.ephemerisRequest.has_value()
           && skygate::ephemeris::EphemerisComputationPolicy::trailSamplingIsAdaptive(
               *input.ephemerisEngine, *input.ephemerisRequest
           );
}

bool SkyObjectTrailSampler::isFixedEquatorialTrailTarget(const SkyObjectTrailSamplingInput& input) noexcept
{
    if (!usesAdaptiveTrailSampling(input) || input.ephemerisEngine == nullptr
        || !skygate::ephemeris::EphemerisComputationPolicy::trailAllowsGuidanceApproximation(*input.ephemerisEngine)
        || input.targetBody == nullptr || input.targetState == nullptr || !input.targetState->equatorial.isFinite()) {
        return false;
    }

    return input.targetBody->fixedEquatorialValue().has_value() || input.targetBody->starAstrometryValue().has_value()
           || input.targetBody->kind == skygate::ephemeris::BaseCelestialBody::Kind::Star;
}

bool SkyObjectTrailSampler::shouldUseGuidanceTrail(const SkyObjectTrailSamplingInput& input) noexcept
{
    return input.ephemerisEngine != nullptr && input.ephemerisRequest.has_value()
           && skygate::ephemeris::EphemerisComputationPolicy::trailUsesGuidance(
               *input.ephemerisEngine, *input.ephemerisRequest, input.targetBody
           )
           && !isFixedEquatorialTrailTarget(input);
}

SkyObjectTrailSampler::TrailSampleCacheKey
SkyObjectTrailSampler::sampleCacheKeyFor(const SkyObjectTrailSamplingInput& input)
{
    const skygate::core::ObservationContext& context =
        input.ephemerisRequest.has_value() ? input.ephemerisRequest->context : input.skyContext;
    return TrailSampleCacheKey{
        .ephemerisEngine = input.ephemerisEngine,
        .guidanceStrategy = input.guidanceStrategy,
        .context = context,
        .requestEpoch = input.ephemerisRequest.has_value() ? std::make_optional(input.ephemerisRequest->epoch)
                                                           : std::optional<skygate::core::AstronomicalEpoch>{},
        .requestOptions = input.ephemerisRequest.has_value()
                              ? std::make_optional(input.ephemerisRequest->options)
                              : std::optional<skygate::ephemeris::EphemerisEngineOptions>{},
        .targetEquatorial = input.targetState != nullptr && input.targetState->equatorial.isFinite()
                                ? std::make_optional(input.targetState->equatorial)
                                : std::optional<skygate::core::EquatorialCoordinate>{},
        .targetBodyIndex = input.targetBodyIndex
    };
}

bool SkyObjectTrailSampler::sampleCacheKeysEqual(
    const TrailSampleCacheKey& lhs, const TrailSampleCacheKey& rhs
) noexcept
{
    return lhs.ephemerisEngine == rhs.ephemerisEngine && lhs.guidanceStrategy == rhs.guidanceStrategy
           && lhs.targetBodyIndex == rhs.targetBodyIndex && lhs.context.utcTime == rhs.context.utcTime
           && lhs.context.observer.equals(rhs.context.observer) && lhs.requestEpoch == rhs.requestEpoch
           && lhs.requestOptions == rhs.requestOptions
           && optionalEquatorialsEqual(lhs.targetEquatorial, rhs.targetEquatorial);
}

bool SkyObjectTrailSampler::hasCachedSamples(const SkyObjectTrailSamplingInput& input) const
{
    return m_sampleCacheKey.has_value() && sampleCacheKeysEqual(*m_sampleCacheKey, sampleCacheKeyFor(input));
}

const std::vector<skygate::ephemeris::BodyTrailSample>&
SkyObjectTrailSampler::samples(const SkyObjectTrailSamplingInput& input)
{
    const TrailSampleCacheKey cacheKey = sampleCacheKeyFor(input);
    if (!m_sampleCacheKey.has_value() || !sampleCacheKeysEqual(*m_sampleCacheKey, cacheKey)) {
        const skygate::ephemeris::BodyTrailCalculator trailCalculator;
        m_sampleCache = sampleTrail(input, trailCalculator);
        m_sampleCacheKey = cacheKey;
    }

    return m_sampleCache;
}
