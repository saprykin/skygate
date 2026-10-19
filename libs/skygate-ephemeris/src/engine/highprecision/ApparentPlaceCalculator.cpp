#include "ApparentPlaceCalculator.hpp"
#include "ApparentPlaceOutputFinalizer.hpp"
#include "FrameTransformBatcher.hpp"

#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace skygate::ephemeris::highprecision {

ApparentPlaceCalculator::ApparentPlaceCalculator(
    std::shared_ptr<const IFrameTransformer> frameTransformer,
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider,
    std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator
)
    : m_frameTransformer(std::move(frameTransformer)), m_timeScaleService(std::move(timeScaleService)),
      m_earthOrientationProvider(std::move(earthOrientationProvider)),
      m_atmosphericRefractionCalculator(std::move(atmosphericRefractionCalculator))
{
}

std::vector<StarAstrometryBatchResult> ApparentPlaceCalculator::processItems(
    const EphemerisRequest& request,
    const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState,
    const std::span<FrameTransformBatcher::Item> items
) const
{
    FrameTransformBatcher(m_frameTransformer.get(), m_timeScaleService.get(), m_earthOrientationProvider.get())
        .transform(request, preparedRequestState, items);
    return ApparentPlaceOutputFinalizer(m_atmosphericRefractionCalculator.get())
        .finalize(request, preparedRequestState, items);
}

HighPrecisionCalculatorResult ApparentPlaceCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    std::vector<FrameTransformBatcher::Item> items{
        FrameTransformBatcher::Item{
            .body = &input.body,
            .bodyIndex = input.bodyIndex,
            .result = calculatorResult,
        },
    };

    return processItems(input.request, input.preparedRequestState, items).front().result;
}

std::vector<StarAstrometryBatchResult> ApparentPlaceCalculator::applyBatch(
    const EphemerisRequest& request,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    std::vector<FrameTransformBatcher::Item> items;
    items.reserve(calculatorResults.size());
    for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        if (calculatorResult.bodyIndex >= bodies.size()) {
            continue;
        }
        const BaseCelestialBody* body = bodies[calculatorResult.bodyIndex];
        if (body == nullptr) {
            continue;
        }
        items.push_back(
            FrameTransformBatcher::Item{
                .body = body,
                .bodyIndex = calculatorResult.bodyIndex,
                .result = calculatorResult.result,
            }
        );
    }

    return processItems(request, preparedRequestState, items);
}

}  // namespace skygate::ephemeris::highprecision
