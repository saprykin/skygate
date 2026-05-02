#include "skygate/ephemeris/BodyTrailCalculator.hpp"

#include <chrono>
#include <cstddef>

namespace skygate::ephemeris {

std::vector<BodyTrailSample> BodyTrailCalculator::sample(
    const IEphemerisEngine& engine,
    const core::SkyContext& context,
    const std::uint32_t bodyIndex,
    const BodyTrailOptions& options
) const
{
    std::vector<BodyTrailSample> samples;
    if (options.pastHours < 0 || options.futureHours < 0 || options.sampleStepMinutes <= 0) {
        return samples;
    }

    const int startOffsetMinutes = -options.pastHours * 60;
    const int endOffsetMinutes = options.futureHours * 60;
    samples.reserve(
        static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / options.sampleStepMinutes)
        + 1U
    );

    for (
        int offsetMinutes = startOffsetMinutes;
        offsetMinutes <= endOffsetMinutes;
        offsetMinutes += options.sampleStepMinutes
    ) {
        core::SkyContext sampleContext = context;
        sampleContext.utcTime += std::chrono::minutes(offsetMinutes);

        BodyTrailSample sample {
            .offsetMinutes = offsetMinutes,
            .horizontal = std::nullopt
        };

        const auto bodyState = engine.computeBodyState(sampleContext, bodyIndex);
        if (bodyState.has_value() && bodyState->horizontal.isFinite()) {
            sample.horizontal = bodyState->horizontal;
        }

        samples.push_back(sample);
    }

    return samples;
}

}  // namespace skygate::ephemeris
