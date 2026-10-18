#include "SkySceneComposer.hpp"
#include "CelestialReferenceCalculator.hpp"
#include "SkyOverlayItem.hpp"
#include "SkySceneCompositionInput.hpp"
#include "SkySceneCompositionResult.hpp"
#include "SkySceneFrameData.hpp"
#include "SkySceneFramePipelineResult.hpp"
#include "diagnostics/SkyPerformanceLogging.hpp"
#include "diagnostics/SkyPerformanceStageTimer.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "math/Geometry2d.hpp"
#include "render/SkyCardinalDirectionPolicy.hpp"
#include "render/SkyRenderLabel.hpp"
#include "selection/SkyObjectTrailInput.hpp"
#include "selection/SkySelectionOverlayInput.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QColor>
#include <QPointF>

#include <limits>

namespace {

constexpr int kReferenceLabelSampleCount = skygate::ephemeris::CelestialReferenceCalculator::kReferenceCircleSamples;
constexpr double kReferenceLabelEdgeMarginPx = 36.0;

SkyOverlayItem
overlayEntry(const QString& kind, const double x, const double y, const QString& text, const QColor& color)
{
    return SkyOverlayItem{.kind = kind, .x = x, .y = y, .text = text, .color = color};
}

struct TrailTargetPointers final {
    const skygate::ephemeris::BaseCelestialBody* body = nullptr;
    const skygate::ephemeris::CelestialBodyState* state = nullptr;
};

[[nodiscard]] TrailTargetPointers
trailTargetPointers(const skygate::ephemeris::EphemerisSnapshot* snapshot, const std::uint32_t bodyIndex) noexcept
{
    if (snapshot == nullptr || bodyIndex >= snapshot->bodies().size()) {
        return {};
    }

    for (const skygate::ephemeris::CelestialBodyState& state : snapshot->states) {
        if (state.bodyIndex == bodyIndex) {
            return TrailTargetPointers{.body = &snapshot->bodyAt(bodyIndex), .state = &state};
        }
    }

    return {};
}

bool pointIsInsideLabelMargin(
    const skygate::core::ScreenPoint& point, const skygate::core::ProjectionParams& params
) noexcept
{
    return point.x >= kReferenceLabelEdgeMarginPx && point.x <= (params.viewportWidth - kReferenceLabelEdgeMarginPx)
           && point.y >= kReferenceLabelEdgeMarginPx
           && point.y <= (params.viewportHeight - kReferenceLabelEdgeMarginPx);
}

template <typename CoordinateAt>
void appendReferenceLayerLabel(
    std::vector<SkyOverlayItem>& overlayItems,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& text,
    const QColor& color,
    CoordinateAt coordinateAt
)
{
    const auto& params = preparedProjection.params();
    const double targetX = params.viewportWidth * 0.72;
    const double targetY = params.viewportHeight * 0.34;

    std::optional<QPointF> bestPoint;
    double bestScore = std::numeric_limits<double>::max();
    std::optional<QPointF> fallbackPoint;
    double fallbackScore = std::numeric_limits<double>::max();

    for (int index = 0; index < kReferenceLabelSampleCount; ++index) {
        const auto projected = preparedProjection.project(coordinateAt(index));
        if (!projected.isVisible()) {
            continue;
        }

        const double score = skygate::core::Geometry2d::squaredDistance2d(projected.x, projected.y, targetX, targetY);
        if (score < fallbackScore) {
            fallbackPoint = QPointF(projected.x, projected.y);
            fallbackScore = score;
        }
        if (pointIsInsideLabelMargin(projected, params) && score < bestScore) {
            bestPoint = QPointF(projected.x, projected.y);
            bestScore = score;
        }
    }

    const std::optional<QPointF>& labelPoint = bestPoint.has_value() ? bestPoint : fallbackPoint;
    if (!labelPoint.has_value()) {
        return;
    }

    overlayItems.push_back(overlayEntry("referenceLine", labelPoint->x(), labelPoint->y(), text, color));
}

std::vector<SkyOverlayItem> renderLabelsToOverlayItems(const std::span<const SkyRenderLabel> labels)
{
    std::vector<SkyOverlayItem> overlayItems;
    overlayItems.reserve(labels.size());
    for (const SkyRenderLabel& label : labels) {
        overlayItems.push_back(
            SkyOverlayItem{.kind = label.kind, .x = label.x, .y = label.y, .text = label.text, .color = label.color}
        );
    }
    return overlayItems;
}

}  // namespace

namespace {

std::optional<skygate::ephemeris::EphemerisRequest> trailEphemerisRequestFor(
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine,
    const std::optional<skygate::ephemeris::EphemerisRequest>& selectionRequest,
    const std::optional<skygate::ephemeris::EphemerisRequest>& frameRequest
) noexcept
{
    if (selectionRequest.has_value()) {
        if (ephemerisEngine == nullptr) {
            return selectionRequest;
        }
        return skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
            *selectionRequest, skygate::ephemeris::EphemerisPrecisionPolicy::Trail, *ephemerisEngine
        );
    }

    return frameRequest;
}

}  // namespace

bool SkySceneComposer::CompositionKey::equals(const CompositionKey& other) const noexcept
{
    return renderFrameGeneration == other.renderFrameGeneration && trailTargetBodyIndex == other.trailTargetBodyIndex
           && selectedObjectTargetId == other.selectedObjectTargetId
           && selectedSearchTargetKind == other.selectedSearchTargetKind
           && selectedSearchTargetId == other.selectedSearchTargetId && trackedTargetKind == other.trackedTargetKind
           && trackedTargetId == other.trackedTargetId && inspectorPinnedX == other.inspectorPinnedX
           && inspectorPinnedY == other.inspectorPinnedY && inspectorPinned == other.inspectorPinned;
}

void SkySceneComposer::reset()
{
    m_compositionKey.reset();
}

SkySceneCompositionResult SkySceneComposer::rebuild(
    SkySceneFrameData& sceneFrame, const SkySceneCompositionInput& input, const SkySceneFramePipelineResult& frameResult
)
{
    skygate::ui::SkyPerformanceStageTimer timer;

    const SkySelectionOverlayInput selectionInput = buildSelectionInput(input, frameResult);
    const double selectionInputMs = timer.stageMilliseconds();
    const auto trailTargetBodyIndex = m_selectionOverlayBuilder.activeTrailTargetBodyIndex(selectionInput);
    const double trailResolveMs = timer.stageMilliseconds();
    const CompositionKey compositionKey{
        .renderFrameGeneration = frameResult.renderFrameGeneration,
        .trailTargetBodyIndex = trailTargetBodyIndex,
        .selectedObjectTargetId = input.selectedObjectTargetId,
        .selectedSearchTargetKind = input.selectedSearchTargetKind,
        .selectedSearchTargetId = input.selectedSearchTargetId,
        .trackedTargetKind = input.trackedTargetKind,
        .trackedTargetId = input.trackedTargetId,
        .inspectorPinnedX = input.inspectorPinnedX,
        .inspectorPinnedY = input.inspectorPinnedY,
        .inspectorPinned = input.inspectorPinned
    };
    if (!frameResult.updated && m_compositionKey.has_value() && m_compositionKey.value().equals(compositionKey)) {
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "scene composition skipped elapsedMs=" << timer.elapsedMilliseconds();
        }
        return {};
    }

    const bool frameContentChanged = frameResult.updated || !m_compositionKey.has_value()
                                     || m_compositionKey->renderFrameGeneration != compositionKey.renderFrameGeneration
                                     || m_compositionKey->trailTargetBodyIndex != compositionKey.trailTargetBodyIndex;
    double frameCopyMs = 0.0;
    double trailMs = 0.0;
    double overlayMs = 0.0;
    if (frameContentChanged) {
        sceneFrame.frame = *frameResult.frame;
        frameCopyMs = timer.stageMilliseconds();
        if (trailTargetBodyIndex.has_value()) {
            const TrailTargetPointers trailTarget = trailTargetPointers(frameResult.snapshot, *trailTargetBodyIndex);
            m_objectTrailBuilder.appendTrail(
                sceneFrame.frame,
                SkyObjectTrailInput{
                    .ephemerisEngine = input.frameInput.ephemerisEngine,
                    .preparedProjection = frameResult.preparedProjection,
                    .targetBody = trailTarget.body,
                    .targetState = trailTarget.state,
                    .skyContext = input.frameInput.skyContext,
                    .ephemerisRequest = trailEphemerisRequestFor(
                        input.frameInput.ephemerisEngine,
                        input.selectionEphemerisRequest,
                        input.frameInput.ephemerisRequest
                    ),
                    .renderTheme = input.frameInput.renderTheme,
                    .targetBodyIndex = *trailTargetBodyIndex,
                    .viewportWidth = input.viewportWidth,
                    .viewportHeight = input.viewportHeight,
                    .guidanceStrategy = input.guidanceStrategy
                }
            );
        }
        trailMs = timer.stageMilliseconds();

        sceneFrame.overlayItems = buildOverlayItems(sceneFrame, input, frameResult);
        overlayMs = timer.stageMilliseconds();
    }

    sceneFrame.selectionMarker = m_selectionOverlayBuilder.buildSelectionMarkerData(selectionInput);
    const double markerMs = timer.stageMilliseconds();
    sceneFrame.selectedObjectInspector = m_selectionOverlayBuilder.buildSelectedObjectInspectorData(selectionInput);
    const double inspectorMs = timer.stageMilliseconds();
    m_compositionKey = compositionKey;
    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "scene composition elapsedMs=" << timer.elapsedMilliseconds() << "selectionInputMs=" << selectionInputMs
            << "trailResolveMs=" << trailResolveMs << "frameCopyMs=" << frameCopyMs << "trailMs=" << trailMs
            << "overlayMs=" << overlayMs << "markerMs=" << markerMs << "inspectorMs=" << inspectorMs
            << "frameContentChanged=" << frameContentChanged << "hasTrail=" << trailTargetBodyIndex.has_value();
    }
    return SkySceneCompositionResult{
        .changed = true, .frameContentChanged = frameContentChanged, .trailTargetBodyIndex = trailTargetBodyIndex
    };
}

std::vector<SkyOverlayItem> SkySceneComposer::buildOverlayItems(
    const SkySceneFrameData& sceneFrame,
    const SkySceneCompositionInput& input,
    const SkySceneFramePipelineResult& frameResult
) const
{
    std::vector<SkyOverlayItem> overlayItems = renderLabelsToOverlayItems(sceneFrame.frame.labels);

    if (!sceneFrame.preparedProjection.has_value()) {
        return overlayItems;
    }

    const auto& overlayLayers = input.frameInput.overlayLayers;
    const auto& renderTheme = input.frameInput.renderTheme;
    const auto* snapshot = frameResult.snapshot != nullptr ? frameResult.snapshot : sceneFrame.snapshot;
    const auto& skyContext = snapshot != nullptr ? snapshot->context : input.frameInput.skyContext;

    if (overlayLayers.ecliptic) {
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Ecliptic",
            renderTheme.eclipticLine,
            [&skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::eclipticCirclePoint(
                    index, kReferenceLabelSampleCount, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.celestialEquator) {
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Celestial equator",
            renderTheme.celestialEquatorLine,
            [&skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index, kReferenceLabelSampleCount, 0.0, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.circumpolarBoundary && skyContext.observer.isValid()) {
        const double boundaryDeclinationDeg =
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(skyContext.observer);
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Circumpolar",
            renderTheme.circumpolarBoundaryLine,
            [boundaryDeclinationDeg, &skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index, kReferenceLabelSampleCount, boundaryDeclinationDeg, skyContext.observer, skyContext.utcTime
                );
            }
        );
    }

    if (!overlayLayers.horizon) {
        return overlayItems;
    }

    for (const double cardinalAzimuthDeg : skygate::ui::internal::SkyCardinalDirectionPolicy::kAzimuthsDeg) {
        const auto projected = sceneFrame.preparedProjection->project(
            skygate::core::HorizontalCoordinate{.altitudeDeg = 0.0, .azimuthDeg = cardinalAzimuthDeg}
        );
        if (!projected.isVisible()) {
            continue;
        }

        overlayItems.push_back(overlayEntry(
            "cardinal",
            projected.x,
            projected.y,
            skygate::ui::internal::SkyCardinalDirectionPolicy::label(cardinalAzimuthDeg),
            skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(cardinalAzimuthDeg, renderTheme)
        ));
    }

    return overlayItems;
}

SkySelectionOverlayInput SkySceneComposer::buildSelectionInput(
    const SkySceneCompositionInput& input, const SkySceneFramePipelineResult& frameResult
) const
{
    return SkySelectionOverlayInput{
        .snapshot = frameResult.snapshot,
        .ephemerisEngine = input.frameInput.ephemerisEngine,
        .timeController = input.timeController,
        .preparedProjection = frameResult.preparedProjection,
        .stateIndexByBodyId = frameResult.stateIndexByBodyId,
        .skyContext = input.frameInput.skyContext,
        .ephemerisRequest = input.selectionEphemerisRequest,
        .constellationAnchorGroups = input.frameInput.constellationAnchorGroups,
        .catalogSourceIds = input.catalogSourceIds,
        .catalogSourceContributors = input.catalogSourceContributors,
        .catalogSourceTitles = input.catalogSourceTitles,
        .selectedObjectTargetId = input.selectedObjectTargetId,
        .selectedSearchTargetKind = input.selectedSearchTargetKind,
        .selectedSearchTargetId = input.selectedSearchTargetId,
        .trackedTargetKind = input.trackedTargetKind,
        .trackedTargetId = input.trackedTargetId,
        .inspectorPinnedX = input.inspectorPinnedX,
        .inspectorPinnedY = input.inspectorPinnedY,
        .inspectorPinned = input.inspectorPinned,
        .guidanceStrategy = input.guidanceStrategy
    };
}
