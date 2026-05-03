#include "SkySceneModel.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QPointF>
#include <QVariantMap>

#include "skygate/core/math/Geometry2d.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr int kReferenceLabelSampleCount = 96;
constexpr double kReferenceLabelEdgeMarginPx = 36.0;

QVariantMap overlayEntry(
    const QString& kind,
    const double x,
    const double y,
    const QString& text,
    const QColor& color
)
{
    QVariantMap entry;
    entry.insert("kind", kind);
    entry.insert("x", x);
    entry.insert("y", y);
    entry.insert("text", text);
    entry.insert("color", color);
    return entry;
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
    QVariantList& overlayItems,
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

}  // namespace

SkySceneModel::SkySceneModel(QObject* parent)
    : QObject(parent)
{
}

QObject* SkySceneModel::skyContextController() const noexcept
{
    return m_skyContextController;
}

void SkySceneModel::setSkyContextController(QObject* skyContextController)
{
    SkyContextController* controller = qobject_cast<SkyContextController*>(skyContextController);
    if (m_skyContextController == controller) {
        return;
    }

    disconnectFromContextController();
    m_skyContextController = controller;
    static_cast<void>(m_framePipeline.clear());
    m_hitTargetIndex.clear();
    m_sceneCompositionKey.reset();
    m_sceneFrame = {};
    m_selectedObjectTargetId.clear();
    m_selectedObjectInspectorPinned = false;
    if (m_skyContextController != nullptr) {
        m_skyContextChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::skyContextChanged,
            this,
            &SkySceneModel::rebuildSceneFrame
        );
        m_selectedSearchTargetChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::selectedSearchTargetChanged,
            this,
            [this] {
                m_selectedObjectTargetId.clear();
                m_selectedObjectInspectorPinned = false;
                m_sceneCompositionKey.reset();
                rebuildSceneFrame();
            }
        );
        m_trackedTargetChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::trackedTargetChanged,
            this,
            [this] {
                m_sceneCompositionKey.reset();
                rebuildSceneFrame();
            }
        );
        m_themeChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::themeChanged,
            this,
            &SkySceneModel::rebuildSceneFrame
        );
    }

    emit skyContextControllerChanged();
    rebuildSceneFrame();
}

QVariantList SkySceneModel::overlayItems() const
{
    return m_sceneFrame.overlayItems;
}

QVariantMap SkySceneModel::selectionMarker() const
{
    return m_sceneFrame.selectionMarker;
}

QVariantMap SkySceneModel::selectedObjectInspector() const
{
    return m_sceneFrame.selectedObjectInspector;
}

std::uint64_t SkySceneModel::snapshotGeneration() const noexcept
{
    return m_framePipeline.snapshotGeneration();
}

void SkySceneModel::setViewportSize(const double viewportWidth, const double viewportHeight)
{
    if (
        std::abs(m_viewportWidth - viewportWidth) < 1e-9
        && std::abs(m_viewportHeight - viewportHeight) < 1e-9
    ) {
        return;
    }

    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    rebuildSceneFrame();
}

QString SkySceneModel::objectLabelAt(const double x, const double y) const
{
    if (m_sceneFrame.snapshot == nullptr) {
        return {};
    }

    const auto bodyIndex = m_hitTargetIndex.bodyIndexAt(
        x,
        y,
        m_viewportWidth,
        m_viewportHeight,
        *m_sceneFrame.snapshot
    );
    if (!bodyIndex.has_value()) {
        return {};
    }

    const auto& body = m_sceneFrame.snapshot->bodyAt(*bodyIndex);
    return QString::fromStdString(body.displayName);
}

bool SkySceneModel::selectObjectAt(const double x, const double y)
{
    if (m_sceneFrame.snapshot == nullptr) {
        clearSelectedObjectInspector();
        return false;
    }

    const auto bodyIndex = m_hitTargetIndex.bodyIndexAt(
        x,
        y,
        m_viewportWidth,
        m_viewportHeight,
        *m_sceneFrame.snapshot
    );
    if (!bodyIndex.has_value()) {
        clearSelectedObjectInspector();
        return false;
    }

    const auto& body = m_sceneFrame.snapshot->bodyAt(*bodyIndex);
    if (body.id.empty()) {
        clearSelectedObjectInspector();
        return false;
    }

    if (m_skyContextController != nullptr) {
        m_skyContextController->clearSelectedSearchTarget();
    }
    m_selectedObjectTargetId = QString::fromStdString(body.id);
    m_selectedObjectInspectorPinned = false;
    m_sceneCompositionKey.reset();
    rebuildSceneFrame();
    return true;
}

void SkySceneModel::clearSelectedObjectInspector()
{
    const bool hadSelection = !m_selectedObjectTargetId.isEmpty()
        || m_selectedObjectInspectorPinned
        || !m_sceneFrame.selectedObjectInspector.isEmpty();
    m_selectedObjectTargetId.clear();
    m_selectedObjectInspectorPinned = false;
    if (!hadSelection) {
        return;
    }

    m_sceneCompositionKey.reset();
    rebuildSceneFrame();
}

void SkySceneModel::setSelectedObjectInspectorPinned(const bool pinned)
{
    if (m_selectedObjectInspectorPinned == pinned) {
        return;
    }

    if (pinned) {
        if (m_sceneFrame.selectedObjectInspector.isEmpty()) {
            return;
        }

        m_selectedObjectInspectorPinnedX =
            m_sceneFrame.selectedObjectInspector.value("x").toDouble();
        m_selectedObjectInspectorPinnedY =
            m_sceneFrame.selectedObjectInspector.value("y").toDouble();
    }

    m_selectedObjectInspectorPinned = pinned;
    m_sceneCompositionKey.reset();
    rebuildSceneFrame();
}

void SkySceneModel::moveSelectedObjectInspector(const double x, const double y)
{
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return;
    }

    m_selectedObjectInspectorPinnedX = x;
    m_selectedObjectInspectorPinnedY = y;
    m_selectedObjectInspectorPinned = true;
    m_sceneCompositionKey.reset();
    rebuildSceneFrame();
}

std::optional<skygate::core::PreparedProjection> SkySceneModel::preparedProjection() const
{
    return m_sceneFrame.preparedProjection;
}

std::span<const SkyRenderPoint> SkySceneModel::renderPointSpan() const
{
    return std::span<const SkyRenderPoint>(m_sceneFrame.frame.points);
}

std::span<const SkyRenderLine> SkySceneModel::renderLineSpan() const
{
    return std::span<const SkyRenderLine>(m_sceneFrame.frame.lines);
}

std::span<const SkyRenderGlyph> SkySceneModel::renderGlyphSpan() const
{
    return std::span<const SkyRenderGlyph>(m_sceneFrame.frame.glyphs);
}

void SkySceneModel::disconnectFromContextController()
{
    if (m_skyContextChangedConnection) {
        disconnect(m_skyContextChangedConnection);
    }
    if (m_selectedSearchTargetChangedConnection) {
        disconnect(m_selectedSearchTargetChangedConnection);
    }
    if (m_trackedTargetChangedConnection) {
        disconnect(m_trackedTargetChangedConnection);
    }
    if (m_themeChangedConnection) {
        disconnect(m_themeChangedConnection);
    }

    m_skyContextChangedConnection = {};
    m_selectedSearchTargetChangedConnection = {};
    m_trackedTargetChangedConnection = {};
    m_themeChangedConnection = {};
}

bool SkySceneModel::clearSceneFrame()
{
    const bool hadSceneFrame = m_framePipeline.clear()
        || m_sceneFrame.preparedProjection.has_value()
        || m_sceneFrame.snapshot != nullptr
        || !m_sceneFrame.frame.points.empty()
        || !m_sceneFrame.frame.lines.empty()
        || !m_sceneFrame.frame.glyphs.empty()
        || !m_sceneFrame.overlayItems.isEmpty()
        || !m_sceneFrame.selectionMarker.isEmpty()
        || !m_sceneFrame.selectedObjectInspector.isEmpty();
    m_hitTargetIndex.clear();
    m_sceneCompositionKey.reset();
    m_sceneFrame = {};
    return hadSceneFrame;
}

std::optional<SkySceneModel::SceneBuildInput> SkySceneModel::buildSceneInput() const
{
    if (
        m_skyContextController == nullptr
        || m_viewportWidth <= 0.0
        || m_viewportHeight <= 0.0
    ) {
        return std::nullopt;
    }

    const auto* ephemerisEngine = m_skyContextController->ephemerisEngine();
    if (ephemerisEngine == nullptr) {
        return std::nullopt;
    }

    return SceneBuildInput {
        .frameInput = SkySceneFramePipelineInput {
            .ephemerisEngine = ephemerisEngine,
            .skyContext = m_skyContextController->skyContext(),
            .catalogRevision = m_skyContextController->catalogRevision(),
            .projectionType = m_skyContextController->projectionType(),
            .viewCenterAltitudeDeg = m_skyContextController->viewCenterAltitudeDeg(),
            .viewCenterAzimuthDeg = m_skyContextController->viewCenterAzimuthDeg(),
            .viewFieldOfViewDeg = m_skyContextController->viewFieldOfViewDeg(),
            .magnitudeCutoff = m_skyContextController->magnitudeCutoff(),
            .themeId = m_skyContextController->themeId(),
            .renderTheme = m_skyContextController->renderTheme(),
            .overlayLayers = m_skyContextController->overlayLayerVisibility(),
            .constellationLineRefs = m_skyContextController->constellationLineRefs(),
            .constellationLabelRefs = m_skyContextController->constellationLabelRefs()
        },
        .catalogSourceIds = m_skyContextController->catalogSourceIds(),
        .catalogSourceLabels = m_skyContextController->catalogSourceLabels(),
        .selectedSearchTargetKind = m_skyContextController->selectedSearchTargetKind(),
        .selectedSearchTargetId = m_skyContextController->selectedSearchTargetId(),
        .trackedTargetKind = m_skyContextController->trackedTargetKind(),
        .trackedTargetId = m_skyContextController->trackedTargetId()
    };
}

void SkySceneModel::rebuildSceneFrame()
{
    const auto input = buildSceneInput();
    if (!input.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    const auto frameResult = m_framePipeline.rebuild(
        input->frameInput,
        m_viewportWidth,
        m_viewportHeight
    );
    if (!frameResult.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    m_sceneFrame.preparedProjection = *frameResult->preparedProjection;
    m_sceneFrame.snapshot = frameResult->snapshot;

    const SkySelectionOverlayInput selectionInput = buildSelectionInput(*input, *frameResult);
    const auto trailTargetBodyIndex =
        m_selectionOverlayBuilder.activeTrailTargetBodyIndex(selectionInput);
    const SceneCompositionKey compositionKey {
        .renderFrameGeneration = frameResult->renderFrameGeneration,
        .trailTargetBodyIndex = trailTargetBodyIndex,
        .selectedObjectTargetId = m_selectedObjectTargetId,
        .selectedSearchTargetKind = input->selectedSearchTargetKind,
        .selectedSearchTargetId = input->selectedSearchTargetId,
        .trackedTargetKind = input->trackedTargetKind,
        .trackedTargetId = input->trackedTargetId,
        .inspectorPinnedX = m_selectedObjectInspectorPinnedX,
        .inspectorPinnedY = m_selectedObjectInspectorPinnedY,
        .inspectorPinned = m_selectedObjectInspectorPinned
    };
    if (
        !frameResult->updated
        && m_sceneCompositionKey.has_value()
        && m_sceneCompositionKey.value().equals(compositionKey)
    ) {
        return;
    }

    m_sceneFrame.frame = *frameResult->frame;
    if (trailTargetBodyIndex.has_value()) {
        m_objectTrailBuilder.appendTrail(
            m_sceneFrame.frame,
            SkyObjectTrailInput {
                .ephemerisEngine = input->frameInput.ephemerisEngine,
                .preparedProjection = frameResult->preparedProjection,
                .skyContext = input->frameInput.skyContext,
                .renderTheme = input->frameInput.renderTheme,
                .targetBodyIndex = *trailTargetBodyIndex,
                .viewportWidth = m_viewportWidth,
                .viewportHeight = m_viewportHeight
            }
        );
    }

    m_hitTargetIndex.rebuild(m_sceneFrame.frame, *frameResult->snapshot);
    m_sceneFrame.overlayItems = buildOverlayItems(m_sceneFrame, *input);
    m_sceneFrame.selectionMarker =
        m_selectionOverlayBuilder.buildSelectionMarker(selectionInput);
    m_sceneFrame.selectedObjectInspector =
        m_selectionOverlayBuilder.buildSelectedObjectInspector(selectionInput);
    m_sceneCompositionKey = compositionKey;
    emit sceneFrameChanged();
}

QVariantList SkySceneModel::buildOverlayItems(
    const SceneFrameData& sceneFrame,
    const SceneBuildInput& input
) const
{
    QVariantList overlayItems = sceneFrame.frame.labels;

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

SkySelectionOverlayInput SkySceneModel::buildSelectionInput(
    const SceneBuildInput& input,
    const SkySceneFramePipelineResult& frameResult
) const
{
    return SkySelectionOverlayInput {
        .snapshot = frameResult.snapshot,
        .preparedProjection = frameResult.preparedProjection,
        .stateIndexByBodyId = frameResult.stateIndexByBodyId,
        .constellationLabelRefs = input.frameInput.constellationLabelRefs,
        .catalogSourceIds = input.catalogSourceIds,
        .catalogSourceLabels = input.catalogSourceLabels,
        .selectedObjectTargetId = m_selectedObjectTargetId,
        .selectedSearchTargetKind = input.selectedSearchTargetKind,
        .selectedSearchTargetId = input.selectedSearchTargetId,
        .trackedTargetKind = input.trackedTargetKind,
        .trackedTargetId = input.trackedTargetId,
        .inspectorPinnedX = m_selectedObjectInspectorPinnedX,
        .inspectorPinnedY = m_selectedObjectInspectorPinnedY,
        .inspectorPinned = m_selectedObjectInspectorPinned
    };
}
