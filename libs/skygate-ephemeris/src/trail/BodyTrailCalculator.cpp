#include "BodyTrailCalculator.hpp"
#include "CelestialBodyState.hpp"
#include "EphemerisRequestFactory.hpp"
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

struct SampleWindow final {
    int startOffsetMinutes = 0;
    int endOffsetMinutes = 0;
    int stepMinutes = 0;

    [[nodiscard]] static SampleWindow fromOptions(const BodyTrailOptions& options) noexcept
    {
        return SampleWindow{
            .startOffsetMinutes = -options.pastHours * static_cast<int>(TimeConstants::kMinutesPerHour),
            .endOffsetMinutes = options.futureHours * static_cast<int>(TimeConstants::kMinutesPerHour),
            .stepMinutes = options.sampleStepMinutes
        };
    }

    [[nodiscard]] std::size_t sampleCapacity() const noexcept
    {
        return static_cast<std::size_t>((endOffsetMinutes - startOffsetMinutes) / stepMinutes) + 1U;
    }
};

template <typename SampleBuilder>
[[nodiscard]] std::vector<BodyTrailSample>
collectSamples(const BodyTrailOptions& options, SampleBuilder&& sampleBuilder)
{
    std::vector<BodyTrailSample> samples = BodyTrailCalculator::sampleOffsets(options);
    for (BodyTrailSample& sample : samples) {
        sample.horizontal = sampleBuilder(sample.offsetMinutes);
    }

    return samples;
}

}  // namespace

std::vector<BodyTrailSample> BodyTrailCalculator::sampleOffsets(const BodyTrailOptions& options)
{
    if (!validateOptions(options)) {
        return {};
    }

    const SampleWindow window = SampleWindow::fromOptions(options);
    std::vector<BodyTrailSample> samples;
    samples.reserve(window.sampleCapacity());
    for (int offsetMinutes = window.startOffsetMinutes; offsetMinutes <= window.endOffsetMinutes;
         offsetMinutes += window.stepMinutes) {
        samples.push_back(BodyTrailSample{.offsetMinutes = offsetMinutes, .horizontal = std::nullopt});
    }

    return samples;
}

std::optional<skygate::core::HorizontalCoordinate> BodyTrailCalculator::sampleAtOffset(
    const IEphemerisEngine& engine,
    const EphemerisRequest& request,
    const std::uint32_t bodyIndex,
    const int offsetMinutes
)
{
    const skygate::core::UtcTimePoint sampleUtcTime = request.context.utcTime + std::chrono::minutes(offsetMinutes);
    const EphemerisRequest sampleRequest = EphemerisRequestFactory::atUtcTime(request, sampleUtcTime);

    const std::optional<CelestialBodyState> bodyState =
        engine.computeBodyState(sampleRequest, static_cast<std::size_t>(bodyIndex));
    if (!bodyState.has_value() || !bodyState->horizontal.isFinite()) {
        return std::nullopt;
    }

    return bodyState->horizontal;
}

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
        return sampleAtOffset(engine, request, bodyIndex, offsetMinutes);
    });
}

}  // namespace skygate::ephemeris
