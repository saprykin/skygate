#include "PreparedRequestStateBuilder.hpp"
#include "EarthOrientationSampler.hpp"
#include "EphemerisMetadataFailurePolicy.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "ICalcephKernel.hpp"
#include "IEarthOrientationProvider.hpp"
#include "ITimeScaleService.hpp"
#include "ObserverGeodesy.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"

#include <cstdint>
#include <memory>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr int kNaifEarth = 399;
constexpr int kNaifSolarSystemBarycenter = 0;

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

void mergeKernelEpochTimeScaleMetadata(
    EphemerisEngineQueryResult& metadata, const TimeScaleConversionResult& conversion
) noexcept
{
    constexpr std::uint32_t kTdbApproximationWarning =
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::TdbApproximationApplied);
    if (conversion.status == TimeScaleConversionStatus::Degraded
        && (conversion.warningCodeMask & ~kTdbApproximationWarning) == 0U) {
        return;
    }

    EphemerisMetadataMerger::mergeTimeScale(metadata, conversion);
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
    {
        if (request.epoch.timeScale == TimeScale::Tdb) {
            preparedState->tdbKernelEpoch = request.epoch.normalized();
        } else if (m_dependencies.timeScaleService == nullptr) {
            preparedState->tdbKernelEpochMetadata.status = EphemerisEngineQueryStatus::Type::Degraded;
            preparedState->tdbKernelEpochMetadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        } else {
            const TimeScaleConversionResult conversion =
                m_dependencies.timeScaleService->convert(request.epoch, TimeScale::Tdb);
            mergeKernelEpochTimeScaleMetadata(preparedState->tdbKernelEpochMetadata, conversion);
            if (conversion.isSuccess()) {
                preparedState->tdbKernelEpoch = conversion.epoch.normalized();
            }
        }

        if (requestsAnnualParallaxState(request) && preparedState->tdbKernelEpoch.has_value()
            && m_dependencies.calcephKernel != nullptr) {
            preparedState->annualParallaxEarthState = m_dependencies.calcephKernel->compute(
                *preparedState->tdbKernelEpoch, kNaifEarth, kNaifSolarSystemBarycenter
            );
        }
    }

    if (requestsTopocentricState(request) && m_dependencies.timeScaleService != nullptr) {
        preparedState->topocentricStatePrepared = true;
        preparedState->observerItrsPositionAu = ObserverGeodesy::observerItrsPositionAu(request.context.observer);
        const TimeScaleConversionResult utcConversion =
            m_dependencies.timeScaleService->convert(request.epoch, TimeScale::Utc);
        EphemerisMetadataMerger::mergeTimeScale(
            preparedState->topocentricMetadata, utcConversion, EphemerisMetadataFailurePolicy::MarkFailed
        );
        if (!utcConversion.isSuccess()) {
            preparedState->topocentricStateAvailable = false;
            EphemerisMetadataMerger::markCorrectionUnavailable(
                preparedState->topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
            );
        } else {
            preparedState->earthOrientationSample = EarthOrientationSampler::sample(
                m_dependencies.earthOrientationProvider,
                utcConversion.epoch,
                EarthOrientationSampler::Options{
                    .allowOutOfRangeNearestSampleFallback = true,
                    .allowMissingDataZeroFallback = true,
                    .degradePredictedData = false,
                }
            );
            EphemerisMetadataMerger::mergeEarthOrientation(
                preparedState->topocentricMetadata, *preparedState->earthOrientationSample
            );
            if (!preparedState->earthOrientationSample->isSuccess()) {
                preparedState->topocentricStateAvailable = false;
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    preparedState->topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
                );
            }
        }
    }

    return preparedState;
}

}  // namespace skygate::ephemeris::highprecision
