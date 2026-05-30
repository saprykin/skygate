#include "ApparentPlaceCalculator.hpp"
#include "CelestialFrameMath.hpp"
#include "EarthOrientationProvider.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "IFrameTransformer.hpp"
#include "ObserverGeodesy.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr EphemerisMetadataMergeOptions kTransformMetadataMergeOptions{
    .statusPolicy = EphemerisMetadataStatusMergePolicy::DegradedAndFailedOnly,
    .mergeCorrections = true,
    .mergeProvenance = true,
    .mergeValidityRange = false,
    .mergeAngularUncertainty = false,
};

enum class ApparentPlaceRequestMode : std::uint8_t {
    Geometric,
    Astrometric,
    Apparent,
    Topocentric
};

[[nodiscard]] ApparentPlaceRequestMode requestModeForCorrections(const EphemerisCorrectionFlags flags) noexcept
{
    if (flags == EphemerisCorrectionFlags::noCorrections()) {
        return ApparentPlaceRequestMode::Geometric;
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::diurnalParallax())) {
        return ApparentPlaceRequestMode::Topocentric;
    }
    if (skygate::ephemeris::EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::precessionNutation())) {
        return ApparentPlaceRequestMode::Apparent;
    }

    return ApparentPlaceRequestMode::Astrometric;
}

[[nodiscard]] CelestialReferenceFrame::Type targetFrameForRequest(const ApparentPlaceRequestMode mode) noexcept
{
    switch (mode) {
    case ApparentPlaceRequestMode::Geometric:
    case ApparentPlaceRequestMode::Astrometric:
        return CelestialReferenceFrame::Type::Gcrs;
    case ApparentPlaceRequestMode::Apparent:
        return CelestialReferenceFrame::Type::TrueEquatorAndEquinox;
    case ApparentPlaceRequestMode::Topocentric:
        return CelestialReferenceFrame::Type::Itrs;
    }

    return CelestialReferenceFrame::Type::Gcrs;
}

void markMissingBatchTransformResult(
    EphemerisEngineQueryResult& metadata, const EphemerisCorrectionFlags unavailableCorrection
) noexcept
{
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
    metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    metadata.addUnavailableCorrection(unavailableCorrection);
}

[[nodiscard]] skygate::core::Vector3d
computationVectorFromCalculatorResult(const HighPrecisionCalculatorResult& calculatorResult) noexcept
{
    if (calculatorResult.observerRelativePositionAu.has_value()
        && calculatorResult.observerRelativePositionAu->isFinite()) {
        return *calculatorResult.observerRelativePositionAu;
    }

    return CelestialFrameMath::fromEquatorial(*calculatorResult.equatorial);
}

[[nodiscard]] CelestialFrameTransformResult transformSingleVector(
    const IFrameTransformer& frameTransformer,
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const AstronomicalEpoch& epoch,
    const skygate::core::Vector3d& vector
)
{
    const std::array<skygate::core::Vector3d, 1U> vectors{vector};
    const std::vector<CelestialFrameTransformResult> results = frameTransformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = sourceFrame,
            .targetFrame = targetFrame,
            .epoch = epoch,
            .vectors = vectors,
        }
    );
    if (!results.empty()) {
        return results.front();
    }

    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
    return result;
}

}  // namespace

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

HighPrecisionCalculatorResult ApparentPlaceCalculator::apply(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    HighPrecisionCalculatorResult result = calculatorResult;
    if (!calculatorResult.equatorial.has_value() || !calculatorResult.equatorial->isFinite()) {
        return result;
    }
    const EphemerisCorrectionFlags requestedCorrections = input.request.options.correctionFlags();
    const ApparentPlaceRequestMode requestMode = requestModeForCorrections(requestedCorrections);
    const CelestialReferenceFrame::Type targetFrame = targetFrameForRequest(requestMode);
    if (targetFrame == CelestialReferenceFrame::Type::Itrs) {
        if (input.preparedRequestState != nullptr && input.preparedRequestState->topocentricStatePrepared) {
            EphemerisMetadataMerger::merge(
                result.metadata, input.preparedRequestState->topocentricMetadata, kTransformMetadataMergeOptions
            );
            if (!input.preparedRequestState->topocentricStateAvailable) {
                return result;
            }
        } else if (m_timeScaleService == nullptr) {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::earthOrientation());
            return result;
        }

        const TimeScaleConversionResult utcConversion =
            m_timeScaleService->convert(input.request.epoch, TimeScale::Utc);
        EphemerisMetadataMerger::mergeTimeScale(
            result.metadata, utcConversion, EphemerisMetadataFailurePolicy::MarkFailed
        );
        if (!utcConversion.isSuccess()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::earthOrientation()
            );
            return result;
        }

        const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
            m_earthOrientationProvider,
            utcConversion.epoch,
            EarthOrientationSampleOptions{
                .allowOutOfRangeNearestSampleFallback = true,
                .allowMissingDataZeroFallback = true,
                .degradePredictedData = false,
            }
        );
        EphemerisMetadataMerger::mergeEarthOrientation(result.metadata, earthOrientationSample);
        if (!earthOrientationSample.isSuccess()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::earthOrientation()
            );
            return result;
        }
    }

    skygate::core::Vector3d outputVector = computationVectorFromCalculatorResult(calculatorResult);
    if (m_frameTransformer != nullptr) {
        CelestialFrameTransformResult transformResult = transformSingleVector(
            *m_frameTransformer, CelestialReferenceFrame::Type::Gcrs, targetFrame, input.request.epoch, outputVector
        );
        EphemerisMetadataMerger::merge(result.metadata, transformResult.metadata, kTransformMetadataMergeOptions);

        if (!transformResult.vector.has_value()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::precessionNutation()
            );
            return result;
        }
        outputVector = *transformResult.vector;
    } else if (targetFrame != CelestialReferenceFrame::Type::Gcrs) {
        EphemerisMetadataMerger::markCorrectionUnavailable(
            result.metadata,
            targetFrame == CelestialReferenceFrame::Type::Itrs
                ? (EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::earthOrientation())
                : EphemerisCorrectionFlags::precessionNutation()
        );
        return result;
    }
    if (targetFrame == CelestialReferenceFrame::Type::Itrs) {
        const std::optional<skygate::core::Vector3d> observerPosition =
            input.preparedRequestState != nullptr && input.preparedRequestState->topocentricStatePrepared
                ? input.preparedRequestState->observerItrsPositionAu
                : observerItrsPositionAu(input.request.context.observer);
        if (!observerPosition.has_value()) {
            if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
                result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
            }
            result.metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
            result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::diurnalParallax());
        } else if (calculatorResult.observerRelativePositionAu.has_value()) {
            outputVector -= *observerPosition;
            result.metadata.appliedCorrections |= EphemerisCorrectionFlags::diurnalParallax();
        } else {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::diurnalParallax()
            );
        }
    }

    std::optional<skygate::core::Vector3d> equatorialVector =
        targetFrame == CelestialReferenceFrame::Type::Itrs ? std::nullopt
                                                           : std::optional<skygate::core::Vector3d>{outputVector};
    if (targetFrame == CelestialReferenceFrame::Type::Itrs) {
        CelestialFrameTransformResult gcrsTransformResult = transformSingleVector(
            *m_frameTransformer,
            CelestialReferenceFrame::Type::Itrs,
            CelestialReferenceFrame::Type::Gcrs,
            input.request.epoch,
            outputVector
        );
        EphemerisMetadataMerger::merge(result.metadata, gcrsTransformResult.metadata, kTransformMetadataMergeOptions);
        if (gcrsTransformResult.vector.has_value()) {
            equatorialVector = *gcrsTransformResult.vector;
        } else {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::earthOrientation()
            );
        }

        if (equatorialVector.has_value()
            && skygate::ephemeris::EphemerisCorrectionFlags::has(
                requestedCorrections, EphemerisCorrectionFlags::precessionNutation()
            )) {
            CelestialFrameTransformResult apparentEquatorialTransformResult = transformSingleVector(
                *m_frameTransformer,
                CelestialReferenceFrame::Type::Gcrs,
                CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
                input.request.epoch,
                *equatorialVector
            );
            EphemerisMetadataMerger::merge(
                result.metadata, apparentEquatorialTransformResult.metadata, kTransformMetadataMergeOptions
            );
            if (apparentEquatorialTransformResult.vector.has_value()) {
                equatorialVector = *apparentEquatorialTransformResult.vector;
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::precessionNutation()
                );
            }
        }
    }

    if (equatorialVector.has_value()) {
        if (const std::optional<skygate::core::EquatorialCoordinate> equatorial =
                CelestialFrameMath::toEquatorial(*equatorialVector);
            equatorial.has_value()) {
            result.equatorial = *equatorial;
        } else {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
            return result;
        }
    }

    if (targetFrame == CelestialReferenceFrame::Type::Itrs) {
        if (const std::optional<skygate::core::HorizontalCoordinate> horizontal =
                CelestialFrameMath::horizontalFromItrsVector(outputVector, input.request.context.observer);
            horizontal.has_value()) {
            result.horizontal = *horizontal;
        } else {
            if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
                result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
            }
            result.metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
        }
    }

    if (input.request.options.enableAtmosphericRefraction()
        && skygate::ephemeris::EphemerisCorrectionFlags::has(
            requestedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        )) {
        if (m_atmosphericRefractionCalculator == nullptr) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
            );
        } else {
            result = m_atmosphericRefractionCalculator->apply(input, result);
        }
    }

    return result;
}

std::vector<StarAstrometryBatchResult> ApparentPlaceCalculator::applyBatch(
    const EphemerisRequest& request,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    const EphemerisCorrectionFlags requestedCorrections = request.options.correctionFlags();
    const ApparentPlaceRequestMode requestMode = requestModeForCorrections(requestedCorrections);
    const CelestialReferenceFrame::Type targetFrame = targetFrameForRequest(requestMode);
    const bool isTopocentric = targetFrame == CelestialReferenceFrame::Type::Itrs;
    const bool requestsAtmosphericRefraction =
        request.options.enableAtmosphericRefraction()
        && skygate::ephemeris::EphemerisCorrectionFlags::has(
            requestedCorrections, EphemerisCorrectionFlags::atmosphericRefraction()
        );

    EphemerisEngineQueryResult topocentricMetadata;
    bool topocentricRequestWideStateAvailable = true;
    if (isTopocentric) {
        if (preparedRequestState != nullptr && preparedRequestState->topocentricStatePrepared) {
            topocentricMetadata = preparedRequestState->topocentricMetadata;
            topocentricRequestWideStateAvailable = preparedRequestState->topocentricStateAvailable;
        } else if (m_timeScaleService == nullptr) {
            topocentricMetadata.status = EphemerisEngineQueryStatus::Type::Failed;
            topocentricMetadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            topocentricMetadata.addUnavailableCorrection(EphemerisCorrectionFlags::earthOrientation());
            topocentricRequestWideStateAvailable = false;
        } else {
            const TimeScaleConversionResult utcConversion = m_timeScaleService->convert(request.epoch, TimeScale::Utc);
            EphemerisMetadataMerger::mergeTimeScale(
                topocentricMetadata, utcConversion, EphemerisMetadataFailurePolicy::MarkFailed
            );
            if (!utcConversion.isSuccess()) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
                );
                topocentricRequestWideStateAvailable = false;
            } else {
                const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
                    m_earthOrientationProvider,
                    utcConversion.epoch,
                    EarthOrientationSampleOptions{
                        .allowOutOfRangeNearestSampleFallback = true,
                        .allowMissingDataZeroFallback = true,
                        .degradePredictedData = false,
                    }
                );
                EphemerisMetadataMerger::mergeEarthOrientation(topocentricMetadata, earthOrientationSample);
                if (!earthOrientationSample.isSuccess()) {
                    EphemerisMetadataMerger::markCorrectionUnavailable(
                        topocentricMetadata, EphemerisCorrectionFlags::earthOrientation()
                    );
                    topocentricRequestWideStateAvailable = false;
                }
            }
        }
    }

    std::vector<StarAstrometryBatchResult> results;
    results.reserve(calculatorResults.size());
    std::vector<std::optional<skygate::core::Vector3d>> outputVectors;
    outputVectors.reserve(calculatorResults.size());
    std::vector<bool> hasObserverRelativePosition;
    hasObserverRelativePosition.reserve(calculatorResults.size());
    std::vector<skygate::core::Vector3d> transformInputs;
    transformInputs.reserve(calculatorResults.size());
    std::vector<std::size_t> transformResultIndices;
    transformResultIndices.reserve(calculatorResults.size());

    for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        if (calculatorResult.bodyIndex >= bodies.size()) {
            continue;
        }

        HighPrecisionCalculatorResult result = calculatorResult.result;
        if (!calculatorResult.result.equatorial.has_value() || !calculatorResult.result.equatorial->isFinite()) {
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = std::move(result),
                }
            );
            outputVectors.push_back(std::nullopt);
            hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
            continue;
        }
        if (isTopocentric) {
            EphemerisMetadataMerger::merge(result.metadata, topocentricMetadata, kTransformMetadataMergeOptions);
            if (!topocentricRequestWideStateAvailable) {
                results.push_back(
                    StarAstrometryBatchResult{
                        .bodyIndex = calculatorResult.bodyIndex,
                        .result = std::move(result),
                    }
                );
                outputVectors.push_back(std::nullopt);
                hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
                continue;
            }
        }

        skygate::core::Vector3d outputVector = computationVectorFromCalculatorResult(calculatorResult.result);
        if (targetFrame == CelestialReferenceFrame::Type::Gcrs) {
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = std::move(result),
                }
            );
            outputVectors.push_back(outputVector);
            hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
            continue;
        }

        results.push_back(
            StarAstrometryBatchResult{
                .bodyIndex = calculatorResult.bodyIndex,
                .result = std::move(result),
            }
        );
        outputVectors.push_back(std::nullopt);
        hasObserverRelativePosition.push_back(calculatorResult.result.observerRelativePositionAu.has_value());
        if (m_frameTransformer != nullptr) {
            transformInputs.push_back(outputVector);
            transformResultIndices.push_back(results.size() - 1U);
        } else {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                results.back().result.metadata,
                isTopocentric
                    ? (EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::earthOrientation())
                    : EphemerisCorrectionFlags::precessionNutation()
            );
        }
    }

    if (m_frameTransformer != nullptr && !transformInputs.empty()) {
        const std::vector<CelestialFrameTransformResult> transformResults = m_frameTransformer->transform(
            CelestialFrameTransformRequest{
                .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
                .targetFrame = targetFrame,
                .epoch = request.epoch,
                .vectors = transformInputs,
            }
        );
        const std::size_t transformCount = std::min(transformResults.size(), transformResultIndices.size());
        for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
            const std::size_t resultIndex = transformResultIndices[transformIndex];
            HighPrecisionCalculatorResult& result = results[resultIndex].result;
            const CelestialFrameTransformResult& transformResult = transformResults[transformIndex];
            EphemerisMetadataMerger::merge(result.metadata, transformResult.metadata, kTransformMetadataMergeOptions);
            if (!transformResult.vector.has_value()) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::precessionNutation()
                );
                continue;
            }

            outputVectors[resultIndex] = *transformResult.vector;
        }
        const EphemerisCorrectionFlags unavailableCorrection =
            isTopocentric
                ? (EphemerisCorrectionFlags::precessionNutation() | EphemerisCorrectionFlags::earthOrientation())
                : EphemerisCorrectionFlags::precessionNutation();
        for (std::size_t transformIndex = transformCount; transformIndex < transformResultIndices.size();
             ++transformIndex) {
            HighPrecisionCalculatorResult& result = results[transformResultIndices[transformIndex]].result;
            markMissingBatchTransformResult(result.metadata, unavailableCorrection);
        }
    }

    std::vector<std::optional<skygate::core::Vector3d>> equatorialVectors =
        isTopocentric ? std::vector<std::optional<skygate::core::Vector3d>>(outputVectors.size()) : outputVectors;
    if (isTopocentric) {
        const std::optional<skygate::core::Vector3d> observerPosition =
            preparedRequestState != nullptr && preparedRequestState->topocentricStatePrepared
                ? preparedRequestState->observerItrsPositionAu
                : observerItrsPositionAu(request.context.observer);
        for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
            if (!outputVectors[resultIndex].has_value()) {
                continue;
            }

            HighPrecisionCalculatorResult& result = results[resultIndex].result;
            if (!observerPosition.has_value()) {
                if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
                    result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
                }
                result.metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
                result.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::diurnalParallax());
            } else if (hasObserverRelativePosition[resultIndex]) {
                outputVectors[resultIndex] = *outputVectors[resultIndex] - *observerPosition;
                result.metadata.appliedCorrections |= EphemerisCorrectionFlags::diurnalParallax();
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::diurnalParallax()
                );
            }

            if (const std::optional<skygate::core::HorizontalCoordinate> horizontal =
                    CelestialFrameMath::horizontalFromItrsVector(*outputVectors[resultIndex], request.context.observer);
                horizontal.has_value()) {
                result.horizontal = *horizontal;
            } else {
                if (result.metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
                    result.metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
                }
                result.metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
            }
        }

        std::vector<skygate::core::Vector3d> gcrsInputs;
        std::vector<std::size_t> gcrsResultIndices;
        gcrsInputs.reserve(results.size());
        gcrsResultIndices.reserve(results.size());
        for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
            if (outputVectors[resultIndex].has_value()) {
                gcrsInputs.push_back(*outputVectors[resultIndex]);
                gcrsResultIndices.push_back(resultIndex);
            }
        }

        if (m_frameTransformer != nullptr && !gcrsInputs.empty()) {
            const std::vector<CelestialFrameTransformResult> gcrsTransformResults = m_frameTransformer->transform(
                CelestialFrameTransformRequest{
                    .sourceFrame = CelestialReferenceFrame::Type::Itrs,
                    .targetFrame = CelestialReferenceFrame::Type::Gcrs,
                    .epoch = request.epoch,
                    .vectors = gcrsInputs,
                }
            );
            const std::size_t transformCount = std::min(gcrsTransformResults.size(), gcrsResultIndices.size());
            for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
                const std::size_t resultIndex = gcrsResultIndices[transformIndex];
                HighPrecisionCalculatorResult& result = results[resultIndex].result;
                const CelestialFrameTransformResult& transformResult = gcrsTransformResults[transformIndex];
                EphemerisMetadataMerger::merge(
                    result.metadata, transformResult.metadata, kTransformMetadataMergeOptions
                );
                if (transformResult.vector.has_value()) {
                    equatorialVectors[resultIndex] = *transformResult.vector;
                } else {
                    EphemerisMetadataMerger::markCorrectionUnavailable(
                        result.metadata, EphemerisCorrectionFlags::earthOrientation()
                    );
                }
            }
            for (std::size_t transformIndex = transformCount; transformIndex < gcrsResultIndices.size();
                 ++transformIndex) {
                HighPrecisionCalculatorResult& result = results[gcrsResultIndices[transformIndex]].result;
                markMissingBatchTransformResult(result.metadata, EphemerisCorrectionFlags::earthOrientation());
            }
        }

        if (skygate::ephemeris::EphemerisCorrectionFlags::has(
                requestedCorrections, EphemerisCorrectionFlags::precessionNutation()
            )) {
            std::vector<skygate::core::Vector3d> apparentInputs;
            std::vector<std::size_t> apparentResultIndices;
            apparentInputs.reserve(results.size());
            apparentResultIndices.reserve(results.size());
            for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
                if (equatorialVectors[resultIndex].has_value()) {
                    apparentInputs.push_back(*equatorialVectors[resultIndex]);
                    apparentResultIndices.push_back(resultIndex);
                }
            }

            if (m_frameTransformer != nullptr && !apparentInputs.empty()) {
                const std::vector<CelestialFrameTransformResult> apparentTransformResults =
                    m_frameTransformer->transform(
                        CelestialFrameTransformRequest{
                            .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
                            .targetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
                            .epoch = request.epoch,
                            .vectors = apparentInputs,
                        }
                    );
                const std::size_t transformCount =
                    std::min(apparentTransformResults.size(), apparentResultIndices.size());
                for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
                    const std::size_t resultIndex = apparentResultIndices[transformIndex];
                    HighPrecisionCalculatorResult& result = results[resultIndex].result;
                    const CelestialFrameTransformResult& transformResult = apparentTransformResults[transformIndex];
                    EphemerisMetadataMerger::merge(
                        result.metadata, transformResult.metadata, kTransformMetadataMergeOptions
                    );
                    if (transformResult.vector.has_value()) {
                        equatorialVectors[resultIndex] = *transformResult.vector;
                    } else {
                        EphemerisMetadataMerger::markCorrectionUnavailable(
                            result.metadata, EphemerisCorrectionFlags::precessionNutation()
                        );
                    }
                }
                for (std::size_t transformIndex = transformCount; transformIndex < apparentResultIndices.size();
                     ++transformIndex) {
                    HighPrecisionCalculatorResult& result = results[apparentResultIndices[transformIndex]].result;
                    markMissingBatchTransformResult(result.metadata, EphemerisCorrectionFlags::precessionNutation());
                }
            }
        }
    }

    for (std::size_t resultIndex = 0U; resultIndex < results.size(); ++resultIndex) {
        HighPrecisionCalculatorResult& result = results[resultIndex].result;
        if (!equatorialVectors[resultIndex].has_value()) {
            continue;
        }

        if (const std::optional<skygate::core::EquatorialCoordinate> equatorial =
                CelestialFrameMath::toEquatorial(*equatorialVectors[resultIndex]);
            equatorial.has_value()) {
            result.equatorial = *equatorial;
        } else {
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        }

        if (requestsAtmosphericRefraction) {
            if (m_atmosphericRefractionCalculator == nullptr) {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
                );
            } else {
                const HighPrecisionComputationInput input{
                    .request = request,
                    .body = *bodies[results[resultIndex].bodyIndex],
                    .preparedRequestState = preparedRequestState,
                    .bodyIndex = results[resultIndex].bodyIndex,
                };
                result = m_atmosphericRefractionCalculator->apply(input, result);
            }
        }
    }

    return results;
}

}  // namespace skygate::ephemeris::highprecision
