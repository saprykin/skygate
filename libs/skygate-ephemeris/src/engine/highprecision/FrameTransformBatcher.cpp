#include "FrameTransformBatcher.hpp"
#include "ApparentPlaceRequestPolicy.hpp"
#include "CelestialFrameMath.hpp"
#include "EphemerisMetadataMergeOptions.hpp"
#include "EphemerisMetadataMerger.hpp"
#include "IFrameTransformer.hpp"
#include "ObserverGeodesy.hpp"
#include "PreparedEphemerisRequestState.hpp"
#include "PreparedRequestStateBuilder.hpp"

#include <algorithm>
#include <optional>
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

void markMissingObserver(
    EphemerisEngineQueryResult& metadata, const std::optional<EphemerisCorrectionFlags> unavailableCorrection = {}
) noexcept
{
    EphemerisMetadataMerger::markDegraded(metadata);
    metadata.addWarning(EphemerisEngineWarning::Code::MissingObserver);
    if (unavailableCorrection.has_value()) {
        EphemerisMetadataMerger::markCorrectionUnavailable(metadata, *unavailableCorrection);
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

}  // namespace

FrameTransformBatcher::FrameTransformBatcher(
    const IFrameTransformer* frameTransformer,
    const skygate::ephemeris::ITimeScaleService* timeScaleService,
    const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
)
    : m_frameTransformer(frameTransformer), m_timeScaleService(timeScaleService),
      m_earthOrientationProvider(earthOrientationProvider)
{
}

void FrameTransformBatcher::transform(
    const EphemerisRequest& request,
    const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState,
    const std::span<Item> items
) const
{
    const CelestialReferenceFrame::Type requestedTargetFrame = targetFrame(request);
    const bool topocentric = requestedTargetFrame == CelestialReferenceFrame::Type::Itrs;

    TopocentricRequestState topocentricState;
    if (topocentric) {
        topocentricState = prepareTopocentricRequestState(request, preparedRequestState.get());
    }

    VectorList transformInputs;
    ItemIndexList transformItemIndices;
    collectPrimaryTransformInputs(requestedTargetFrame, topocentricState, items, transformInputs, transformItemIndices);
    if (m_frameTransformer != nullptr && !transformInputs.empty()) {
        transformVectors(
            request,
            items,
            TransformRequest{
                .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
                .targetFrame = requestedTargetFrame,
                .inputs = transformInputs,
                .itemIndices = transformItemIndices,
                .unavailableCorrection =
                    ApparentPlaceRequestPolicy::primaryTransformUnavailableCorrection(requestedTargetFrame),
                .destination = VectorDestination::Output,
            }
        );
    }

    if (!topocentric) {
        useOutputVectorsAsEquatorialVectors(items);
    } else {
        applyTopocentricCorrections(request, preparedRequestState.get(), items);
    }
}

CelestialReferenceFrame::Type FrameTransformBatcher::targetFrame(const EphemerisRequest& request) noexcept
{
    return ApparentPlaceRequestPolicy::targetFrame(
        ApparentPlaceRequestPolicy::modeForCorrections(request.options.correctionFlags())
    );
}

FrameTransformBatcher::TopocentricRequestState FrameTransformBatcher::prepareTopocentricRequestState(
    const EphemerisRequest& request, const PreparedEphemerisRequestState* requestState
) const
{
    if (requestState != nullptr && requestState->topocentricStatePrepared) {
        return TopocentricRequestState{
            .metadata = requestState->topocentricMetadata,
            .available = requestState->topocentricStateAvailable,
        };
    }

    PreparedEphemerisRequestState preparedState;
    PreparedRequestStateBuilder::prepareTopocentricState(
        request, m_timeScaleService, m_earthOrientationProvider, preparedState
    );
    return TopocentricRequestState{
        .metadata = std::move(preparedState.topocentricMetadata),
        .available = preparedState.topocentricStateAvailable,
    };
}

void FrameTransformBatcher::collectPrimaryTransformInputs(
    const CelestialReferenceFrame::Type requestedTargetFrame,
    const TopocentricRequestState& topocentricState,
    const std::span<Item> items,
    VectorList& transformInputs,
    ItemIndexList& transformItemIndices
) const
{
    const bool topocentric = requestedTargetFrame == CelestialReferenceFrame::Type::Itrs;
    const EphemerisCorrectionFlags unavailableTransformCorrection =
        ApparentPlaceRequestPolicy::primaryTransformUnavailableCorrection(requestedTargetFrame);

    transformInputs.reserve(items.size());
    transformItemIndices.reserve(items.size());

    for (std::size_t itemIndex = 0U; itemIndex < items.size(); ++itemIndex) {
        Item& item = items[itemIndex];
        if (!item.result.equatorial.has_value() || !item.result.equatorial->isFinite()) {
            continue;
        }

        if (topocentric) {
            EphemerisMetadataMerger::merge(
                item.result.metadata, topocentricState.metadata, kTransformMetadataMergeOptions
            );
            if (!topocentricState.available) {
                continue;
            }
        }

        const skygate::core::Vector3d outputVector = computationVectorFromCalculatorResult(item.result);
        if (requestedTargetFrame == CelestialReferenceFrame::Type::Gcrs) {
            item.outputVector = outputVector;
            continue;
        }

        if (m_frameTransformer != nullptr) {
            transformInputs.push_back(outputVector);
            transformItemIndices.push_back(itemIndex);
        } else {
            EphemerisMetadataMerger::markCorrectionUnavailable(item.result.metadata, unavailableTransformCorrection);
        }
    }
}

void FrameTransformBatcher::runVectorPass(
    const EphemerisRequest& request,
    const std::span<Item> items,
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const std::optional<skygate::core::Vector3d> Item::* vectorMember,
    const EphemerisCorrectionFlags unavailableCorrection,
    const VectorDestination destination
) const
{
    if (m_frameTransformer == nullptr) {
        return;
    }

    VectorList inputs;
    ItemIndexList itemIndices;
    inputs.reserve(items.size());
    itemIndices.reserve(items.size());
    for (std::size_t itemIndex = 0U; itemIndex < items.size(); ++itemIndex) {
        const std::optional<skygate::core::Vector3d>& vector = items[itemIndex].*vectorMember;
        if (vector.has_value()) {
            inputs.push_back(*vector);
            itemIndices.push_back(itemIndex);
        }
    }
    if (inputs.empty()) {
        return;
    }

    transformVectors(
        request,
        items,
        TransformRequest{
            .sourceFrame = sourceFrame,
            .targetFrame = targetFrame,
            .inputs = inputs,
            .itemIndices = itemIndices,
            .unavailableCorrection = unavailableCorrection,
            .destination = destination,
        }
    );
}

void FrameTransformBatcher::transformVectors(
    const EphemerisRequest& request, const std::span<Item> items, const TransformRequest& transformRequest
) const
{
    const std::vector<CelestialFrameTransformResult> results = m_frameTransformer->transform(
        CelestialFrameTransformRequest{
            .sourceFrame = transformRequest.sourceFrame,
            .targetFrame = transformRequest.targetFrame,
            .epoch = request.epoch,
            .vectors = transformRequest.inputs,
        }
    );
    const std::size_t transformCount = std::min(results.size(), transformRequest.itemIndices.size());
    for (std::size_t transformIndex = 0U; transformIndex < transformCount; ++transformIndex) {
        Item& item = items[transformRequest.itemIndices[transformIndex]];
        const CelestialFrameTransformResult& transformResult = results[transformIndex];
        EphemerisMetadataMerger::merge(item.result.metadata, transformResult.metadata, kTransformMetadataMergeOptions);
        if (!transformResult.vector.has_value()) {
            clearFailedTransformState(item, transformRequest.destination);
            EphemerisMetadataMerger::markCorrectionUnavailable(
                item.result.metadata, transformRequest.unavailableCorrection
            );
            continue;
        }

        setTransformedVector(item, transformRequest.destination, *transformResult.vector);
    }

    for (std::size_t transformIndex = transformCount; transformIndex < transformRequest.itemIndices.size();
         ++transformIndex) {
        Item& item = items[transformRequest.itemIndices[transformIndex]];
        clearFailedTransformState(item, transformRequest.destination);
        EphemerisMetadataMerger::markCorrectionFailed(item.result.metadata, transformRequest.unavailableCorrection);
    }
}

void FrameTransformBatcher::setTransformedVector(
    Item& item, const VectorDestination destination, const skygate::core::Vector3d& vector
)
{
    switch (destination) {
    case VectorDestination::Output:
        item.outputVector = vector;
        return;
    case VectorDestination::Equatorial:
        item.equatorialVector = vector;
        return;
    }
}

void FrameTransformBatcher::clearFailedTransformState(Item& item, const VectorDestination destination)
{
    switch (destination) {
    case VectorDestination::Output:
        item.outputVector.reset();
        item.equatorialVector.reset();
        item.result.equatorial.reset();
        item.result.horizontal.reset();
        return;
    case VectorDestination::Equatorial:
        item.equatorialVector.reset();
        item.result.equatorial.reset();
        return;
    }
}

void FrameTransformBatcher::useOutputVectorsAsEquatorialVectors(const std::span<Item> items)
{
    for (Item& item : items) {
        item.equatorialVector = item.outputVector;
    }
}

void FrameTransformBatcher::applyTopocentricCorrections(
    const EphemerisRequest& request, const PreparedEphemerisRequestState* requestState, const std::span<Item> items
) const
{
    const std::optional<skygate::core::Vector3d> observerPosition =
        requestState != nullptr && requestState->topocentricStatePrepared
            ? requestState->observerItrsPositionAu
            : ObserverGeodesy::observerItrsPositionAu(request.context.observer);
    for (Item& item : items) {
        if (!item.outputVector.has_value()) {
            continue;
        }

        applyObserverParallax(item, observerPosition);
        updateHorizontalCoordinate(item, request);
    }

    transformItrsVectorsToGcrs(request, items);
    transformGcrsVectorsToApparentWhenRequested(request, items);
}

void FrameTransformBatcher::applyObserverParallax(
    Item& item, const std::optional<skygate::core::Vector3d>& observerPosition
)
{
    if (!observerPosition.has_value()) {
        markMissingObserver(item.result.metadata, EphemerisCorrectionFlags::diurnalParallax());
    } else if (item.result.observerRelativePositionAu.has_value()) {
        item.outputVector = *item.outputVector - *observerPosition;
        EphemerisMetadataMerger::markCorrectionApplied(
            item.result.metadata, EphemerisCorrectionFlags::diurnalParallax()
        );
    } else {
        EphemerisMetadataMerger::markCorrectionUnavailable(
            item.result.metadata, EphemerisCorrectionFlags::diurnalParallax()
        );
    }
}

void FrameTransformBatcher::updateHorizontalCoordinate(Item& item, const EphemerisRequest& request)
{
    if (const std::optional<skygate::core::HorizontalCoordinate> horizontal =
            CelestialFrameMath::horizontalFromItrsVector(*item.outputVector, request.context.observer);
        horizontal.has_value()) {
        item.result.horizontal = *horizontal;
    } else {
        markMissingObserver(item.result.metadata);
    }
}

void FrameTransformBatcher::transformItrsVectorsToGcrs(
    const EphemerisRequest& request, const std::span<Item> items
) const
{
    runVectorPass(
        request,
        items,
        CelestialReferenceFrame::Type::Itrs,
        CelestialReferenceFrame::Type::Gcrs,
        &Item::outputVector,
        EphemerisCorrectionFlags::earthOrientation(),
        VectorDestination::Equatorial
    );
}

void FrameTransformBatcher::transformGcrsVectorsToApparentWhenRequested(
    const EphemerisRequest& request, const std::span<Item> items
) const
{
    if (!EphemerisCorrectionFlags::has(
            request.options.correctionFlags(), EphemerisCorrectionFlags::precessionNutation()
        )) {
        return;
    }

    runVectorPass(
        request,
        items,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
        &Item::equatorialVector,
        EphemerisCorrectionFlags::precessionNutation(),
        VectorDestination::Equatorial
    );
}

}  // namespace skygate::ephemeris::highprecision
