#include "SkySceneComposition.hpp"

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"

#include <QColor>
#include <QPointF>

#include <array>
#include <limits>

namespace {

constexpr int kReferenceLabelSampleCount = 96;
constexpr double kReferenceLabelEdgeMarginPx = 36.0;

SkyOverlayItem overlayEntry(
    const QString& kind,
    const double x,
    const double y,
    const QString& text,
    const QColor& color
)
{
    return SkyOverlayItem {
        .kind = kind,
        .x = x,
        .y = y,
        .text = text,
        .color = color
    };
}

QColor cardinalColor(
    const double azimuthDeg,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    if (azimuthDeg == 0.0) {
        return renderTheme.cardinalNorthLine;
    }
    if (azimuthDeg == 90.0) {
        return renderTheme.cardinalEastLine;
    }
    if (azimuthDeg == 180.0) {
        return renderTheme.cardinalSouthLine;
    }
    return renderTheme.cardinalWestLine;
}

bool pointIsInsideLabelMargin(
    const skygate::core::ScreenPoint& point,
    const skygate::core::ProjectionParams& params
) noexcept
{
    return point.x >= kReferenceLabelEdgeMarginPx
        && point.x <= (params.viewportWidth - kReferenceLabelEdgeMarginPx)
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
        if (!projected.isVisible) {
            continue;
        }

        const double score = skygate::core::squaredDistance2d(
            projected.x,
            projected.y,
            targetX,
            targetY
        );
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

    overlayItems.push_back(overlayEntry(
        "referenceLine",
        labelPoint->x(),
        labelPoint->y(),
        text,
        color
    ));
}

std::vector<SkyOverlayItem> renderLabelsToOverlayItems(
    const std::span<const SkyRenderLabel> labels
)
{
    std::vector<SkyOverlayItem> overlayItems;
    overlayItems.reserve(labels.size());
    for (const SkyRenderLabel& label : labels) {
        overlayItems.push_back(SkyOverlayItem {
            .kind = label.kind,
            .x = label.x,
            .y = label.y,
            .text = label.text,
            .color = label.color
        });
    }
    return overlayItems;
}

}  // namespace

bool SkySceneComposer::CompositionKey::equals(
    const CompositionKey& other
) const noexcept
{
    return renderFrameGeneration == other.renderFrameGeneration
        && trailTargetBodyIndex == other.trailTargetBodyIndex
        && selectedObjectTargetId == other.selectedObjectTargetId
        && selectedSearchTargetKind == other.selectedSearchTargetKind
        && selectedSearchTargetId == other.selectedSearchTargetId
        && trackedTargetKind == other.trackedTargetKind
        && trackedTargetId == other.trackedTargetId
        && inspectorPinnedX == other.inspectorPinnedX
        && inspectorPinnedY == other.inspectorPinnedY
        && inspectorPinned == other.inspectorPinned;
}

void SkySceneComposer::reset()
{
    m_compositionKey.reset();
}

SkySceneCompositionResult SkySceneComposer::rebuild(
    SkySceneFrameData& sceneFrame,
    const SkySceneCompositionInput& input,
    const SkySceneFramePipelineResult& frameResult
)
{
    const SkySelectionOverlayInput selectionInput = buildSelectionInput(input, frameResult);
    const auto trailTargetBodyIndex =
        m_selectionOverlayBuilder.activeTrailTargetBodyIndex(selectionInput);
    const CompositionKey compositionKey {
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
    if (
        !frameResult.updated
        && m_compositionKey.has_value()
        && m_compositionKey.value().equals(compositionKey)
    ) {
        return {};
    }

    const bool frameContentChanged = frameResult.updated
        || !m_compositionKey.has_value()
        || m_compositionKey->renderFrameGeneration != compositionKey.renderFrameGeneration
        || m_compositionKey->trailTargetBodyIndex != compositionKey.trailTargetBodyIndex;
    if (frameContentChanged) {
        sceneFrame.frame = *frameResult.frame;
        if (trailTargetBodyIndex.has_value()) {
            m_objectTrailBuilder.appendTrail(
                sceneFrame.frame,
                SkyObjectTrailInput {
                    .ephemerisEngine = input.frameInput.ephemerisEngine,
                    .preparedProjection = frameResult.preparedProjection,
                    .skyContext = input.frameInput.skyContext,
                    .renderTheme = input.frameInput.renderTheme,
                    .targetBodyIndex = *trailTargetBodyIndex,
                    .viewportWidth = input.viewportWidth,
                    .viewportHeight = input.viewportHeight
                }
            );
        }

        sceneFrame.overlayItems = buildOverlayItems(sceneFrame, input);
    }

    sceneFrame.selectionMarker =
        m_selectionOverlayBuilder.buildSelectionMarkerData(selectionInput);
    sceneFrame.selectedObjectInspector =
        m_selectionOverlayBuilder.buildSelectedObjectInspectorData(selectionInput);
    m_compositionKey = compositionKey;
    return SkySceneCompositionResult {
        .changed = true,
        .frameContentChanged = frameContentChanged
    };
}

std::vector<SkyOverlayItem> SkySceneComposer::buildOverlayItems(
    const SkySceneFrameData& sceneFrame,
    const SkySceneCompositionInput& input
) const
{
    std::vector<SkyOverlayItem> overlayItems = renderLabelsToOverlayItems(sceneFrame.frame.labels);

    if (!sceneFrame.preparedProjection.has_value()) {
        return overlayItems;
    }

    const auto& overlayLayers = input.frameInput.overlayLayers;
    const auto& renderTheme = input.frameInput.renderTheme;
    const auto& skyContext = input.frameInput.skyContext;

    if (overlayLayers.ecliptic) {
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Ecliptic",
            renderTheme.eclipticLine,
            [&skyContext](const int index) {
                const double eclipticLongitudeDeg = 360.0 * static_cast<double>(index)
                    / static_cast<double>(kReferenceLabelSampleCount);
                return skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
                    eclipticLongitudeDeg,
                    skyContext.observer,
                    skyContext.utcTime
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
                    index,
                    kReferenceLabelSampleCount,
                    0.0,
                    skyContext.observer,
                    skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.circumpolarBoundary && skyContext.observer.isValid()) {
        const double boundaryDeclinationDeg =
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
                skyContext.observer
            );
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Circumpolar",
            renderTheme.circumpolarBoundaryLine,
            [boundaryDeclinationDeg, &skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index,
                    kReferenceLabelSampleCount,
                    boundaryDeclinationDeg,
                    skyContext.observer,
                    skyContext.utcTime
                );
            }
        );
    }

    if (!overlayLayers.horizon) {
        return overlayItems;
    }

    constexpr std::array<const char*, 4> kCardinalLabels {"N", "E", "S", "W"};
    constexpr std::array<double, 4> kCardinalAzimuths {0.0, 90.0, 180.0, 270.0};
    for (std::size_t index = 0; index < kCardinalLabels.size(); ++index) {
        const auto projected = sceneFrame.preparedProjection->project(
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 0.0,
                .azimuthDeg = kCardinalAzimuths[index]
            }
        );
        if (!projected.isVisible) {
            continue;
        }

        overlayItems.push_back(overlayEntry(
            "cardinal",
            projected.x,
            projected.y,
            QString::fromUtf8(kCardinalLabels[index]),
            cardinalColor(kCardinalAzimuths[index], renderTheme)
        ));
    }

    return overlayItems;
}

SkySelectionOverlayInput SkySceneComposer::buildSelectionInput(
    const SkySceneCompositionInput& input,
    const SkySceneFramePipelineResult& frameResult
) const
{
    return SkySelectionOverlayInput {
        .snapshot = frameResult.snapshot,
        .ephemerisEngine = input.frameInput.ephemerisEngine,
        .timeController = input.timeController,
        .preparedProjection = frameResult.preparedProjection,
        .stateIndexByBodyId = frameResult.stateIndexByBodyId,
        .skyContext = input.frameInput.skyContext,
        .constellationLabelRefs = input.frameInput.constellationLabelRefs,
        .catalogSourceIds = input.catalogSourceIds,
        .catalogSourceLabels = input.catalogSourceLabels,
        .selectedObjectTargetId = input.selectedObjectTargetId,
        .selectedSearchTargetKind = input.selectedSearchTargetKind,
        .selectedSearchTargetId = input.selectedSearchTargetId,
        .trackedTargetKind = input.trackedTargetKind,
        .trackedTargetId = input.trackedTargetId,
        .inspectorPinnedX = input.inspectorPinnedX,
        .inspectorPinnedY = input.inspectorPinnedY,
        .inspectorPinned = input.inspectorPinned
    };
}
