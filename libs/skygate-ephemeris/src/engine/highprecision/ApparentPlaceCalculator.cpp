#include "ApparentPlaceCalculator.hpp"
#include "CelestialFrameMath.hpp"
#include "EarthOrientationProvider.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "IFrameTransformer.hpp"
#include "ObserverGeodesy.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

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

enum class TransformVectorDestination : std::uint8_t {
    Output,
    Equatorial
};

struct ApparentPlaceInputItem {
    const BaseCelestialBody* body = nullptr;
    std::size_t bodyIndex = 0U;
    HighPrecisionCalculatorResult result;
};

struct ApparentPlaceWorkItem {
    ApparentPlaceInputItem input;
    std::optional<skygate::core::Vector3d> outputVector;
    std::optional<skygate::core::Vector3d> equatorialVector;
};

struct TopocentricRequestState {
    EphemerisEngineQueryResult metadata;
    bool available = true;
};

struct ApparentPlaceProcessingContext {
    const EphemerisRequest& request;
    std::span<const ApparentPlaceInputItem> inputItems;
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState;
    const IFrameTransformer* frameTransformer = nullptr;
    const skygate::ephemeris::ITimeScaleService* timeScaleService = nullptr;
    const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider = nullptr;
    const IAtmosphericRefractionCalculator* atmosphericRefractionCalculator = nullptr;
};

struct TransformWorkItemRequest {
    CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
    CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Gcrs;
    std::span<const skygate::core::Vector3d> inputs;
    std::span<const std::size_t> itemIndices;
    EphemerisCorrectionFlags unavailableCorrection;
    TransformVectorDestination destination = TransformVectorDestination::Output;
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

[[nodiscard]] EphemerisCorrectionFlags
primaryTransformUnavailableCorrection(const CelestialReferenceFrame::Type targetFrame) noexcept
{
    EphemerisCorrectionFlags correction = EphemerisCorrectionFlags::precessionNutation();
    if (targetFrame == CelestialReferenceFrame::Type::Itrs) {
        correction |= EphemerisCorrectionFlags::earthOrientation();
    }

    return correction;
}

void markMissingObserver(
    EphemerisEngineQueryResult& metadata, const std::optional<EphemerisCorrectionFlags> unavailableCorrection = {}
) noexcept
{
    if (metadata.status == EphemerisEngineQueryStatus::Type::Valid) {
        metadata.status = EphemerisEngineQueryStatus::Type::Degraded;
    }
    metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
    if (unavailableCorrection.has_value()) {
        metadata.addUnavailableCorrection(*unavailableCorrection);
    }
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

void setTransformedVector(
    ApparentPlaceWorkItem& item, const TransformVectorDestination destination, const skygate::core::Vector3d& vector
)
{
    switch (destination) {
    case TransformVectorDestination::Output:
        item.outputVector = vector;
        return;
    case TransformVectorDestination::Equatorial:
        item.equatorialVector = vector;
        return;
    }
}

void clearFailedTransformState(ApparentPlaceWorkItem& item, const TransformVectorDestination destination)
{
    switch (destination) {
    case TransformVectorDestination::Output:
        item.outputVector.reset();
        item.equatorialVector.reset();
        item.input.result.equatorial.reset();
        item.input.result.horizontal.reset();
        return;
    case TransformVectorDestination::Equatorial:
        item.equatorialVector.reset();
        item.input.result.equatorial.reset();
        return;
    }
}

class ApparentPlaceProcessor {
    using ItemIndexList = std::vector<std::size_t>;
    using VectorList = std::vector<skygate::core::Vector3d>;

public:
    explicit ApparentPlaceProcessor(ApparentPlaceProcessingContext context) : m_context(std::move(context)) {}

    [[nodiscard]] std::vector<StarAstrometryBatchResult> process()
    {
        const CelestialReferenceFrame::Type requestedTargetFrame = targetFrame();
        const bool topocentric = requestedTargetFrame == CelestialReferenceFrame::Type::Itrs;

        if (topocentric) {
            m_topocentricState = prepareTopocentricRequestState();
        }

        VectorList transformInputs;
        ItemIndexList transformItemIndices;
        collectPrimaryTransformInputs(requestedTargetFrame, transformInputs, transformItemIndices);
        if (m_context.frameTransformer != nullptr && !transformInputs.empty()) {
            const EphemerisCorrectionFlags unavailableCorrection =
                primaryTransformUnavailableCorrection(requestedTargetFrame);
            transformWorkItemVectors(
                TransformWorkItemRequest{
                    .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
                    .targetFrame = requestedTargetFrame,
                    .inputs = transformInputs,
                    .itemIndices = transformItemIndices,
                    .unavailableCorrection = unavailableCorrection,
                    .destination = TransformVectorDestination::Output,
                }
            );
        }

        if (!topocentric) {
            useOutputVectorsAsEquatorialVectors();
        } else {
            applyTopocentricCorrections();
        }

        return buildResults();
    }

private:
    [[nodiscard]] EphemerisCorrectionFlags requestedCorrections() const noexcept
    {
        return m_context.request.options.correctionFlags();
    }

    [[nodiscard]] CelestialReferenceFrame::Type targetFrame() const noexcept
    {
        return targetFrameForRequest(requestModeForCorrections(requestedCorrections()));
    }

    [[nodiscard]] bool requestsAtmosphericRefraction() const noexcept
    {
        return m_context.request.options.enableAtmosphericRefraction()
               && skygate::ephemeris::EphemerisCorrectionFlags::has(
                   requestedCorrections(), EphemerisCorrectionFlags::atmosphericRefraction()
               );
    }

    [[nodiscard]] TopocentricRequestState prepareTopocentricRequestState() const
    {
        TopocentricRequestState state;
        if (m_context.preparedRequestState != nullptr && m_context.preparedRequestState->topocentricStatePrepared) {
            state.metadata = m_context.preparedRequestState->topocentricMetadata;
            state.available = m_context.preparedRequestState->topocentricStateAvailable;
            return state;
        }

        if (m_context.timeScaleService == nullptr) {
            state.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            state.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
            state.metadata.addUnavailableCorrection(EphemerisCorrectionFlags::earthOrientation());
            state.available = false;
            return state;
        }

        const TimeScaleConversionResult utcConversion =
            m_context.timeScaleService->convert(m_context.request.epoch, TimeScale::Utc);
        EphemerisMetadataMerger::mergeTimeScale(
            state.metadata, utcConversion, EphemerisMetadataFailurePolicy::MarkFailed
        );
        if (!utcConversion.isSuccess()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                state.metadata, EphemerisCorrectionFlags::earthOrientation()
            );
            state.available = false;
            return state;
        }

        const EarthOrientationSample earthOrientationSample = sampleEarthOrientation(
            m_context.earthOrientationProvider,
            utcConversion.epoch,
            EarthOrientationSampleOptions{
                .allowOutOfRangeNearestSampleFallback = true,
                .allowMissingDataZeroFallback = true,
                .degradePredictedData = false,
            }
        );
        EphemerisMetadataMerger::mergeEarthOrientation(state.metadata, earthOrientationSample);
        if (!earthOrientationSample.isSuccess()) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                state.metadata, EphemerisCorrectionFlags::earthOrientation()
            );
            state.available = false;
        }

        return state;
    }

    void collectPrimaryTransformInputs(
        const CelestialReferenceFrame::Type requestedTargetFrame,
        VectorList& transformInputs,
        ItemIndexList& transformItemIndices
    )
    {
        const bool topocentric = requestedTargetFrame == CelestialReferenceFrame::Type::Itrs;
        const EphemerisCorrectionFlags unavailableTransformCorrection =
            primaryTransformUnavailableCorrection(requestedTargetFrame);

        m_items.reserve(m_context.inputItems.size());
        transformInputs.reserve(m_context.inputItems.size());
        transformItemIndices.reserve(m_context.inputItems.size());

        for (const ApparentPlaceInputItem& inputItem : m_context.inputItems) {
            ApparentPlaceWorkItem& item = m_items.emplace_back(
                ApparentPlaceWorkItem{
                    .input = inputItem,
                }
            );

            if (!inputItem.result.equatorial.has_value() || !inputItem.result.equatorial->isFinite()) {
                continue;
            }

            if (topocentric) {
                EphemerisMetadataMerger::merge(
                    item.input.result.metadata, m_topocentricState.metadata, kTransformMetadataMergeOptions
                );
                if (!m_topocentricState.available) {
                    continue;
                }
            }

            skygate::core::Vector3d outputVector = computationVectorFromCalculatorResult(inputItem.result);
            if (requestedTargetFrame == CelestialReferenceFrame::Type::Gcrs) {
                item.outputVector = outputVector;
                continue;
            }

            if (m_context.frameTransformer != nullptr) {
                transformInputs.push_back(outputVector);
                transformItemIndices.push_back(m_items.size() - 1U);
            } else {
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    item.input.result.metadata, unavailableTransformCorrection
                );
            }
        }
    }

    void transformWorkItemVectors(const TransformWorkItemRequest& transformRequest)
    {
        const std::vector<CelestialFrameTransformResult> results = m_context.frameTransformer->transform(
            CelestialFrameTransformRequest{
                .sourceFrame = transformRequest.sourceFrame,
                .targetFrame = transformRequest.targetFrame,
                .epoch = m_context.request.epoch,
                .vectors = transformRequest.inputs,
            }
        );
        const std::size_t transformCount = std::min(results.size(), transformRequest.itemIndices.size());
        for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
            ApparentPlaceWorkItem& item = m_items[transformRequest.itemIndices[transformIndex]];
            const CelestialFrameTransformResult& transformResult = results[transformIndex];
            EphemerisMetadataMerger::merge(
                item.input.result.metadata, transformResult.metadata, kTransformMetadataMergeOptions
            );
            if (!transformResult.vector.has_value()) {
                clearFailedTransformState(item, transformRequest.destination);
                EphemerisMetadataMerger::markCorrectionUnavailable(
                    item.input.result.metadata, transformRequest.unavailableCorrection
                );
                continue;
            }

            setTransformedVector(item, transformRequest.destination, *transformResult.vector);
        }

        for (std::size_t transformIndex = transformCount; transformIndex < transformRequest.itemIndices.size();
             ++transformIndex) {
            ApparentPlaceWorkItem& item = m_items[transformRequest.itemIndices[transformIndex]];
            clearFailedTransformState(item, transformRequest.destination);
            EphemerisMetadataMerger::markCorrectionFailed(
                item.input.result.metadata, transformRequest.unavailableCorrection
            );
        }
    }

    void useOutputVectorsAsEquatorialVectors()
    {
        for (ApparentPlaceWorkItem& item : m_items) {
            item.equatorialVector = item.outputVector;
        }
    }

    void applyTopocentricCorrections()
    {
        const std::optional<skygate::core::Vector3d> observerPosition =
            m_context.preparedRequestState != nullptr && m_context.preparedRequestState->topocentricStatePrepared
                ? m_context.preparedRequestState->observerItrsPositionAu
                : observerItrsPositionAu(m_context.request.context.observer);
        for (ApparentPlaceWorkItem& item : m_items) {
            if (!item.outputVector.has_value()) {
                continue;
            }

            applyObserverParallax(item, observerPosition);
            updateHorizontalCoordinate(item);
        }

        transformItrsVectorsToGcrs();
        transformGcrsVectorsToApparentWhenRequested();
    }

    void applyObserverParallax(
        ApparentPlaceWorkItem& item, const std::optional<skygate::core::Vector3d>& observerPosition
    ) const
    {
        if (!observerPosition.has_value()) {
            markMissingObserver(item.input.result.metadata, EphemerisCorrectionFlags::diurnalParallax());
        } else if (item.input.result.observerRelativePositionAu.has_value()) {
            item.outputVector = *item.outputVector - *observerPosition;
            EphemerisMetadataMerger::markCorrectionApplied(
                item.input.result.metadata, EphemerisCorrectionFlags::diurnalParallax()
            );
        } else {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                item.input.result.metadata, EphemerisCorrectionFlags::diurnalParallax()
            );
        }
    }

    void updateHorizontalCoordinate(ApparentPlaceWorkItem& item) const
    {
        if (const std::optional<skygate::core::HorizontalCoordinate> horizontal =
                CelestialFrameMath::horizontalFromItrsVector(*item.outputVector, m_context.request.context.observer);
            horizontal.has_value()) {
            item.input.result.horizontal = *horizontal;
        } else {
            markMissingObserver(item.input.result.metadata);
        }
    }

    void transformItrsVectorsToGcrs()
    {
        VectorList gcrsInputs;
        ItemIndexList gcrsItemIndices;
        collectVectorInputs(gcrsInputs, gcrsItemIndices, &ApparentPlaceWorkItem::outputVector);

        if (m_context.frameTransformer == nullptr || gcrsInputs.empty()) {
            return;
        }

        transformWorkItemVectors(
            TransformWorkItemRequest{
                .sourceFrame = CelestialReferenceFrame::Type::Itrs,
                .targetFrame = CelestialReferenceFrame::Type::Gcrs,
                .inputs = gcrsInputs,
                .itemIndices = gcrsItemIndices,
                .unavailableCorrection = EphemerisCorrectionFlags::earthOrientation(),
                .destination = TransformVectorDestination::Equatorial,
            }
        );
    }

    void transformGcrsVectorsToApparentWhenRequested()
    {
        if (!skygate::ephemeris::EphemerisCorrectionFlags::has(
                requestedCorrections(), EphemerisCorrectionFlags::precessionNutation()
            )) {
            return;
        }

        VectorList apparentInputs;
        ItemIndexList apparentItemIndices;
        collectVectorInputs(apparentInputs, apparentItemIndices, &ApparentPlaceWorkItem::equatorialVector);

        if (m_context.frameTransformer == nullptr || apparentInputs.empty()) {
            return;
        }

        transformWorkItemVectors(
            TransformWorkItemRequest{
                .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
                .targetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
                .inputs = apparentInputs,
                .itemIndices = apparentItemIndices,
                .unavailableCorrection = EphemerisCorrectionFlags::precessionNutation(),
                .destination = TransformVectorDestination::Equatorial,
            }
        );
    }

    void collectVectorInputs(
        VectorList& inputs,
        ItemIndexList& itemIndices,
        const std::optional<skygate::core::Vector3d> ApparentPlaceWorkItem::* vectorMember
    ) const
    {
        inputs.reserve(m_items.size());
        itemIndices.reserve(m_items.size());
        for (std::size_t itemIndex = 0U; itemIndex < m_items.size(); ++itemIndex) {
            const std::optional<skygate::core::Vector3d>& vector = m_items[itemIndex].*vectorMember;
            if (vector.has_value()) {
                inputs.push_back(*vector);
                itemIndices.push_back(itemIndex);
            }
        }
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> buildResults()
    {
        std::vector<StarAstrometryBatchResult> results;
        results.reserve(m_items.size());
        for (ApparentPlaceWorkItem& item : m_items) {
            const bool canApplyRefraction = finalizeEquatorialCoordinate(item);
            applyAtmosphericRefraction(item, canApplyRefraction);

            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = item.input.bodyIndex,
                    .result = std::move(item.input.result),
                }
            );
        }

        return results;
    }

    [[nodiscard]] bool finalizeEquatorialCoordinate(ApparentPlaceWorkItem& item) const
    {
        if (!item.equatorialVector.has_value()) {
            return false;
        }

        if (const std::optional<skygate::core::EquatorialCoordinate> equatorial =
                CelestialFrameMath::toEquatorial(*item.equatorialVector);
            equatorial.has_value()) {
            item.input.result.equatorial = *equatorial;
            return true;
        } else {
            item.input.result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            item.input.result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        }

        return false;
    }

    void applyAtmosphericRefraction(ApparentPlaceWorkItem& item, const bool canApplyRefraction) const
    {
        if (!requestsAtmosphericRefraction() || !canApplyRefraction) {
            return;
        }

        if (m_context.atmosphericRefractionCalculator == nullptr) {
            EphemerisMetadataMerger::markCorrectionUnavailable(
                item.input.result.metadata, EphemerisCorrectionFlags::atmosphericRefraction()
            );
            return;
        }

        const HighPrecisionComputationInput input{
            .request = m_context.request,
            .body = *item.input.body,
            .preparedRequestState = m_context.preparedRequestState,
            .bodyIndex = item.input.bodyIndex,
        };
        item.input.result = m_context.atmosphericRefractionCalculator->apply(input, item.input.result);
    }

    ApparentPlaceProcessingContext m_context;
    TopocentricRequestState m_topocentricState;
    std::vector<ApparentPlaceWorkItem> m_items;
};

[[nodiscard]] std::vector<StarAstrometryBatchResult> processApparentPlaceItems(ApparentPlaceProcessingContext context)
{
    ApparentPlaceProcessor processor(std::move(context));
    return processor.process();
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
    const std::array<ApparentPlaceInputItem, 1U> inputItems{{
        ApparentPlaceInputItem{
            .body = &input.body,
            .bodyIndex = input.bodyIndex,
            .result = calculatorResult,
        },
    }};
    const std::vector<StarAstrometryBatchResult> results = processApparentPlaceItems(
        ApparentPlaceProcessingContext{
            .request = input.request,
            .inputItems = inputItems,
            .preparedRequestState = input.preparedRequestState,
            .frameTransformer = m_frameTransformer.get(),
            .timeScaleService = m_timeScaleService.get(),
            .earthOrientationProvider = m_earthOrientationProvider.get(),
            .atmosphericRefractionCalculator = m_atmosphericRefractionCalculator.get(),
        }
    );
    return results.front().result;
}

std::vector<StarAstrometryBatchResult> ApparentPlaceCalculator::applyBatch(
    const EphemerisRequest& request,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::span<const StarAstrometryBatchResult> calculatorResults,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    std::vector<ApparentPlaceInputItem> inputItems;
    inputItems.reserve(calculatorResults.size());
    for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
        if (calculatorResult.bodyIndex >= bodies.size()) {
            continue;
        }
        const BaseCelestialBody* body = bodies[calculatorResult.bodyIndex];
        if (body == nullptr) {
            continue;
        }
        inputItems.push_back(
            ApparentPlaceInputItem{
                .body = body,
                .bodyIndex = calculatorResult.bodyIndex,
                .result = calculatorResult.result,
            }
        );
    }

    return processApparentPlaceItems(
        ApparentPlaceProcessingContext{
            .request = request,
            .inputItems = inputItems,
            .preparedRequestState = std::move(preparedRequestState),
            .frameTransformer = m_frameTransformer.get(),
            .timeScaleService = m_timeScaleService.get(),
            .earthOrientationProvider = m_earthOrientationProvider.get(),
            .atmosphericRefractionCalculator = m_atmosphericRefractionCalculator.get(),
        }
    );
}

}  // namespace skygate::ephemeris::highprecision
