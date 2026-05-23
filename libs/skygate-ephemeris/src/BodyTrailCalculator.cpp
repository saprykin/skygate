#include "BodyTrailCalculator.hpp"

#include <chrono>
#include <cstddef>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] AstronomicalEpoch addMinutes(const AstronomicalEpoch& epoch, const int offsetMinutes) noexcept
{
    return normalizedAstronomicalEpoch(AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 + static_cast<double>(offsetMinutes) / (24.0 * 60.0),
        .timeScale = epoch.timeScale
    });
}

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

    const int startOffsetMinutes = -options.pastHours * 60;
    const int endOffsetMinutes = options.futureHours * 60;
    samples.reserve(static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / options.sampleStepMinutes) + 1U);
    return samples;
}

}  // namespace

std::vector<BodyTrailSample> BodyTrailCalculator::sample(
    const IEphemerisEngine& engine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const BodyTrailOptions& options
) const
{
    std::vector<BodyTrailSample> samples = makeEmptySampleSet(options);
    if (!validateOptions(options)) {
        return samples;
    }

    const int startOffsetMinutes = -options.pastHours * 60;
    const int endOffsetMinutes = options.futureHours * 60;

    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += options.sampleStepMinutes) {
        core::SkyContext sampleContext = context;
        sampleContext.utcTime += std::chrono::minutes(offsetMinutes);

        BodyTrailSample sample{.offsetMinutes = offsetMinutes, .horizontal = std::nullopt};

        const auto bodyState = engine.computeBodyState(sampleContext, bodyIndex);
        if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
            sample.horizontal = bodyState->horizontal;
        }

        samples.push_back(sample);
    }

    return samples;
}

std::vector<BodyTrailSample> BodyTrailCalculator::sample(
    const IEphemerisEngine& engine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const BodyTrailOptions& options
) const
{
    std::vector<BodyTrailSample> samples = makeEmptySampleSet(options);
    if (!validateOptions(options)) {
        return samples;
    }

    const int startOffsetMinutes = -options.pastHours * 60;
    const int endOffsetMinutes = options.futureHours * 60;

    for (int offsetMinutes = startOffsetMinutes; offsetMinutes <= endOffsetMinutes;
         offsetMinutes += options.sampleStepMinutes) {
        EphemerisRequest sampleRequest = request;
        sampleRequest.context.utcTime += std::chrono::minutes(offsetMinutes);
        sampleRequest.epoch = addMinutes(request.epoch, offsetMinutes);

        BodyTrailSample sample{.offsetMinutes = offsetMinutes, .horizontal = std::nullopt};

        const auto bodyState = engine.computeBodyState(sampleRequest, static_cast<std::size_t>(bodyIndex));
        if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
            sample.horizontal = bodyState->horizontal;
        }

        samples.push_back(sample);
    }

    return samples;
}

}  // namespace skygate::ephemeris
