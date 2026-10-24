#include "PreparedRequestStateBuilder.hpp"
#include "EphemerisMetadataFailurePolicy.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "EpochScaleResolver.hpp"
#include "ICalcephKernel.hpp"
#include "NaifBodyIds.hpp"
#include "ObserverGeodesy.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "engine/EarthOrientationSampler.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ITimeScaleService.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

using skygate::core::NaifBodyIds;

[[nodiscard]] bool requestsAnnualParallaxState(const EphemerisRequest& request) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(
        request.options.correctionFlags(), EphemerisCorrectionFlags::annualParallax()
    );
}

[[nodiscard]] bool requestsTopocentricState(const EphemerisRequest& request) noexcept
{
    return skygate::ephemeris::EphemerisCorrectionFlags::has(
        request.options.correctionFlags(), EphemerisCorrectionFlags::diurnalParallax()
    );
}

}  // namespace

PreparedRequestStateBuilder::PreparedRequestStateBuilder(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
}

std::shared_ptr<const PreparedEphemerisRequestState>
PreparedRequestStateBuilder::build(const EphemerisRequest& request) const
{
    auto preparedState = std::make_shared<PreparedEphemerisRequestState>();
    preparedState->tdbKernelEpoch = EpochScaleResolver::resolve(
        preparedState->tdbKernelEpochMetadata,
        request.epoch,
        m_dependencies.timeScaleService,
        EphemerisMetadataFailurePolicy::MarkDegraded
    );

    if (requestsAnnualParallaxState(request) && preparedState->tdbKernelEpoch.has_value()
        && m_dependencies.calcephKernel != nullptr) {
        preparedState->annualParallaxEarthState = m_dependencies.calcephKernel->compute(
            *preparedState->tdbKernelEpoch, NaifBodyIds::kEarth, NaifBodyIds::kSolarSystemBarycenter
        );
    }

    if (requestsTopocentricState(request) && m_dependencies.timeScaleService != nullptr) {
        prepareTopocentricState(
            request,
            m_dependencies.timeScaleService.get(),
            m_dependencies.earthOrientationProvider.get(),
            *preparedState
        );
    }

    return preparedState;
}

void PreparedRequestStateBuilder::prepareTopocentricState(
    const EphemerisRequest& request,
    const skygate::ephemeris::ITimeScaleService* const timeScaleService,
    const skygate::ephemeris::IEarthOrientationProvider* const earthOrientationProvider,
    PreparedEphemerisRequestState& state
)
{
    state.topocentricStatePrepared = true;
    state.observerItrsPositionAu = ObserverGeodesy::observerItrsPositionAu(request.context.observer);
    if (timeScaleService == nullptr) {
        EphemerisMetadataMerger::markTimeScaleUnavailable(
            state.topocentricMetadata, EphemerisMetadataFailurePolicy::MarkFailed
        );
        EphemerisMetadataMerger::markCorrectionUnavailable(
            state.topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
        );
        state.earthOrientationSample.reset();
        state.topocentricStateAvailable = false;
        return;
    }

    state.earthOrientationSample = EphemerisMetadataMerger::prepareEarthOrientation(
        request.epoch,
        timeScaleService,
        earthOrientationProvider,
        state.topocentricMetadata,
        EphemerisMetadataFailurePolicy::MarkFailed,
        EarthOrientationSampler::Options::tolerantFallback()
    );
    state.topocentricStateAvailable = state.earthOrientationSample.has_value();
}

}  // namespace skygate::ephemeris::highprecision
