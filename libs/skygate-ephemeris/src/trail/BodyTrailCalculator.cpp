#include "BodyTrailCalculator.hpp"
#include "CelestialBodyState.hpp"
#include "math/TimeConstants.hpp"

#include <chrono>
#include <cstddef>
#include <optional>

namespace skygate::ephemeris {

using skygate::core::TimeConstants;

namespace {

[[nodiscard]] bool validateOptions(const BodyTrailOptions& options) noexcept
{
    return options.pastHours >= 0 && options.futureHours >= 0 && options.sampleStepMinutes > 0;
}

[[nodiscard]] std::vector<BodyTrailSample> makeEmptySampleSet(const BodyTrailOptions& options)
{
    std::vector<BodyTrailSample> samples;
    if (!validateOptions(options)) {
        return samples;
    }

    const int startOffsetMinutes = -options.pastHours * static_cast<int>(TimeConstants::kMinutesPerHour);
    const int endOffsetMinutes = options.futureHours * static_cast<int>(TimeConstants::kMinutesPerHour);
    samples.reserve(static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / options.sampleStepMinutes) + 1U);
    return samples;
}

template <typename SampleBuilder>
[[nodiscard]] std::vector<BodyTrailSample>
collectSamples(const BodyTrailOptions& options, SampleBuilder&& sampleBuilder)
{
    std::vector<BodyTrailSample> samples = makeEmptySampleSet(options);
    if (!validateOptions(options)) {
        return samples;
    }

    const int startOffsetMinutes = -options.pastHours * static_cast<int>(TimeConstants::kMinutesPerHour);
    const int endOffsetMinutes = options.futureHours * static_cast<int>(TimeConstants::kMinutesPerHour);

    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += options.sampleStepMinutes) {
        samples.push_back(BodyTrailSample{.offsetMinutes = offsetMinutes, .horizontal = sampleBuilder(offsetMinutes)});
    }

    return samples;
}

}  // namespace

std::vector<BodyTrailSample> BodyTrailCalculator::sample(
    const IEphemerisEngine& engine,
    const skygate::core::ObservationContext& context,
    const std::uint32_t bodyIndex,
    const BodyTrailOptions& options
) const
{
    return collectSamples(options, [&engine, &context, bodyIndex](const int offsetMinutes) {
        skygate::core::ObservationContext sampleContext = context;
        sampleContext.utcTime += std::chrono::minutes(offsetMinutes);

        const std::optional<CelestialBodyState> bodyState = engine.computeBodyState(sampleContext, bodyIndex);
        if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
            return std::optional<skygate::core::HorizontalCoordinate>{bodyState->horizontal};
        }

        return std::optional<skygate::core::HorizontalCoordinate>{};
    });
}

std::vector<BodyTrailSample> BodyTrailCalculator::sample(
    const IEphemerisEngine& engine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BodyTrailOptions& options
) const
{
    return collectSamples(options, [&engine, &request, bodyIndex](const int offsetMinutes) {
        EphemerisRequest sampleRequest = request;
        sampleRequest.context.utcTime += std::chrono::minutes(offsetMinutes);
        sampleRequest.epoch = request.epoch.addMinutes(offsetMinutes);

        const std::optional<CelestialBodyState> bodyState =
            engine.computeBodyState(sampleRequest, static_cast<std::size_t>(bodyIndex));
        if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
            return std::optional<skygate::core::HorizontalCoordinate>{bodyState->horizontal};
        }

        return std::optional<skygate::core::HorizontalCoordinate>{};
    });
}

}  // namespace skygate::ephemeris
