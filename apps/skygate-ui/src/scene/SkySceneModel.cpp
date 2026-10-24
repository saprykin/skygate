#include "SkySceneModel.hpp"
#include "BaseCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "SkySceneCompositionInput.hpp"
#include "SkySceneCompositionResult.hpp"
#include "SkySceneFramePipelineInput.hpp"
#include "SkyViewportSnapshotInput.hpp"
#include "app/SkyContextController.hpp"
#include "diagnostics/SkyPerformanceLogging.hpp"
#include "diagnostics/SkyPerformanceStageTimer.hpp"
#include "engine/EphemerisPrecisionPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "math/NumericTolerance.hpp"
#include "render/SkyRenderGlyph.hpp"
#include "render/SkyRenderLine.hpp"
#include "render/SkyRenderPoint.hpp"
#include "time/SkyTimeController.hpp"

#include <cmath>
#include <memory>
#include <optional>

SkySceneModel::SkySceneModel(QObject* parent) : QObject(parent) {}

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
    m_sceneComposer.reset();
    m_sceneFrame = {};
    m_viewportSnapshotPublisher.invalidatePublishedFrame();
    m_inspectorPlacement.clear();
    if (m_skyContextController != nullptr) {
        m_skyContextChangedConnection = connect(
            m_skyContextController, &SkyContextController::skyContextChanged, this, &SkySceneModel::rebuildSceneFrame
        );
        m_selectedSearchTargetChangedConnection =
            connect(m_skyContextController, &SkyContextController::selectedSearchTargetChanged, this, [this] {
                if (m_ignoringSearchTargetChange) {
                    return;
                }
                m_inspectorPlacement.clear();
                m_sceneComposer.reset();
                rebuildSceneFrame();
            });
        m_trackedTargetChangedConnection =
            connect(m_skyContextController, &SkyContextController::trackedTargetChanged, this, [this] {
                m_sceneComposer.reset();
                rebuildSceneFrame();
            });
        m_themeChangedConnection = connect(
            m_skyContextController, &SkyContextController::themeChanged, this, &SkySceneModel::rebuildSceneFrame
        );
        m_timeZoneChangedConnection =
            connect(m_skyContextController->timeController(), &SkyTimeController::timeZoneChanged, this, [this] {
                m_sceneComposer.reset();
                rebuildSceneFrame();
            });
    }

    rebuildSceneFrame();
}

QVariantList SkySceneModel::overlayItems() const
{
    return m_overlayItems;
}

QVariantList SkySceneModel::ephemerisDegradationReasons() const
{
    return m_degradationReporter.reasons();
}

QVariantMap SkySceneModel::selectionMarker() const
{
    return m_selectionMarker;
}

QVariantMap SkySceneModel::selectedObjectInspector() const
{
    return m_selectedObjectInspector;
}

std::uint64_t SkySceneModel::snapshotGeneration() const noexcept
{
    return m_framePipeline.snapshotGeneration();
}

std::shared_ptr<const SkyViewportSnapshot> SkySceneModel::viewportSnapshot() const noexcept
{
    return m_viewportSnapshotPublisher.snapshot();
}

void SkySceneModel::setViewportSize(const double viewportWidth, const double viewportHeight)
{
    if (std::abs(m_viewportWidth - viewportWidth) < skygate::core::NumericTolerance::kDisplayChange
        && std::abs(m_viewportHeight - viewportHeight) < skygate::core::NumericTolerance::kDisplayChange) {
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

    const auto bodyIndex =
        m_hitTargetIndex.bodyIndexAt(x, y, m_viewportWidth, m_viewportHeight, *m_sceneFrame.snapshot);
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

    const auto bodyIndex =
        m_hitTargetIndex.bodyIndexAt(x, y, m_viewportWidth, m_viewportHeight, *m_sceneFrame.snapshot);
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
        m_ignoringSearchTargetChange = true;
        m_skyContextController->clearSelectedSearchTarget();
        m_ignoringSearchTargetChange = false;
    }
    m_inspectorPlacement.selectTarget(QString::fromStdString(body.id));
    m_sceneComposer.reset();
    rebuildSceneFrame();
    return true;
}

void SkySceneModel::clearSelectedObjectInspector()
{
    const bool hadSelection = m_inspectorPlacement.hasSelection() || m_sceneFrame.selectedObjectInspector.visible;
    m_inspectorPlacement.clear();
    if (!hadSelection) {
        return;
    }

    m_sceneComposer.reset();
    rebuildSceneFrame();
}

void SkySceneModel::setSelectedObjectInspectorPinned(const bool pinned)
{
    const SkyInspectorPlacement::Change change = m_inspectorPlacement.pin(pinned, m_sceneFrame.selectedObjectInspector);
    if (change == SkyInspectorPlacement::Change::Ignored) {
        return;
    }

    if (change == SkyInspectorPlacement::Change::AppliedInPlace) {
        m_sceneFrame.selectedObjectInspector.pinned = m_inspectorPlacement.pinned();
        m_selectedObjectInspector = m_sceneOverlayAdapter.selectedObjectInspector(m_sceneFrame.selectedObjectInspector);
        emit sceneFrameChanged();
        return;
    }

    m_sceneComposer.reset();
    rebuildSceneFrame();
}

void SkySceneModel::moveSelectedObjectInspector(const double x, const double y)
{
    const SkyInspectorPlacement::Change change = m_inspectorPlacement.move(x, y, m_sceneFrame.selectedObjectInspector);
    if (change == SkyInspectorPlacement::Change::Ignored) {
        return;
    }

    if (change == SkyInspectorPlacement::Change::AppliedInPlace) {
        m_sceneFrame.selectedObjectInspector.x = m_inspectorPlacement.pinnedX();
        m_sceneFrame.selectedObjectInspector.y = m_inspectorPlacement.pinnedY();
        m_sceneFrame.selectedObjectInspector.pinned = true;
        m_selectedObjectInspector = m_sceneOverlayAdapter.selectedObjectInspector(m_sceneFrame.selectedObjectInspector);
        emit sceneFrameChanged();
        return;
    }

    m_sceneComposer.reset();
    rebuildSceneFrame();
}

std::optional<skygate::core::PreparedProjection> SkySceneModel::preparedProjection() const
{
    return m_sceneFrame.preparedProjection;
}

std::optional<skygate::core::ObservationContext> SkySceneModel::referenceOverlayContext() const
{
    if (m_sceneFrame.snapshot == nullptr) {
        return std::nullopt;
    }

    return m_sceneFrame.snapshot->context;
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
    if (m_timeZoneChangedConnection) {
        disconnect(m_timeZoneChangedConnection);
    }

    m_skyContextChangedConnection = {};
    m_selectedSearchTargetChangedConnection = {};
    m_trackedTargetChangedConnection = {};
    m_themeChangedConnection = {};
    m_timeZoneChangedConnection = {};
}

bool SkySceneModel::clearSceneFrame()
{
    const bool hadSceneFrame =
        m_framePipeline.clear() || m_sceneFrame.preparedProjection.has_value() || m_sceneFrame.snapshot != nullptr
        || !m_sceneFrame.frame.points.empty() || !m_sceneFrame.frame.lines.empty() || !m_sceneFrame.frame.glyphs.empty()
        || !m_sceneFrame.overlayItems.empty() || m_sceneFrame.selectionMarker.visible
        || m_sceneFrame.selectedObjectInspector.visible || !m_overlayItems.isEmpty()
        || m_degradationReporter.hasReasons() || !m_selectionMarker.isEmpty() || !m_selectedObjectInspector.isEmpty();
    m_hitTargetIndex.clear();
    m_sceneComposer.reset();
    m_sceneFrame = {};
    m_overlayItems = {};
    m_degradationReporter.clear();
    m_selectionMarker = {};
    m_selectedObjectInspector = {};
    if (m_viewportSnapshotPublisher.clear()) {
        emit viewportSnapshotChanged();
    }
    return hadSceneFrame;
}

void SkySceneModel::publishViewportSnapshot(
    const std::uint64_t renderFrameGeneration, const std::optional<std::uint32_t>& trailTargetBodyIndex
)
{
    SkyViewportSnapshotInput input;
    input.renderFrameGeneration = renderFrameGeneration;
    input.trailTargetBodyIndex = trailTargetBodyIndex;
    input.viewportWidth = m_viewportWidth;
    input.viewportHeight = m_viewportHeight;
    input.preparedProjection = m_sceneFrame.preparedProjection;
    if (m_skyContextController != nullptr) {
        input.renderTheme = m_skyContextController->renderTheme();
        input.overlayLayers = m_skyContextController->overlayLayerVisibility();
        input.observationContext = referenceOverlayContext().value_or(m_skyContextController->skyContext());
    }
    input.points = std::span<const SkyRenderPoint>(m_sceneFrame.frame.points);
    input.lines = std::span<const SkyRenderLine>(m_sceneFrame.frame.lines);
    input.glyphs = std::span<const SkyRenderGlyph>(m_sceneFrame.frame.glyphs);

    if (m_viewportSnapshotPublisher.publish(input)) {
        emit viewportSnapshotChanged();
    }
}

std::optional<SkySceneCompositionInput> SkySceneModel::buildSceneInput() const
{
    if (m_skyContextController == nullptr || m_viewportWidth <= 0.0 || m_viewportHeight <= 0.0) {
        return std::nullopt;
    }

    const auto* ephemerisEngine = m_skyContextController->ephemerisEngine();
    if (ephemerisEngine == nullptr) {
        return std::nullopt;
    }
    const auto ephemerisRequestContext = m_skyContextController->ephemerisRequestContext();
    const auto renderEphemerisRequest = skygate::ephemeris::EphemerisRequest::fromPrecisionPolicy(
        ephemerisRequestContext.request, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender, *ephemerisEngine
    );

    return SkySceneCompositionInput{
        .frameInput =
            SkySceneFramePipelineInput{
                .ephemerisEngine = ephemerisEngine,
                .skyContext = ephemerisRequestContext.request.context,
                .ephemerisRequest = renderEphemerisRequest,
                .catalogRevision = ephemerisRequestContext.catalogRevision,
                .engineKind = ephemerisRequestContext.request.options.engineKind(),
                .engineOptionsRevision = ephemerisRequestContext.engineOptionsRevision,
                .ephemerisDataRevision = ephemerisRequestContext.ephemerisDataRevision,
                .earthOrientationDataRevision = ephemerisRequestContext.earthOrientationDataRevision,
                .leapSecondDataRevision = ephemerisRequestContext.leapSecondDataRevision,
                .projectionType = m_skyContextController->projectionType(),
                .viewCenterAltitudeDeg = m_skyContextController->viewCenterAltitudeDeg(),
                .viewCenterAzimuthDeg = m_skyContextController->viewCenterAzimuthDeg(),
                .viewFieldOfViewDeg = m_skyContextController->viewFieldOfViewDeg(),
                .magnitudeCutoff = m_skyContextController->magnitudeCutoff(),
                .themeId = m_skyContextController->themeId(),
                .renderTheme = m_skyContextController->renderTheme(),
                .overlayLayers = m_skyContextController->overlayLayerVisibility(),
                .constellationLineRefs = m_skyContextController->resolvedConstellationLineRefs(),
                .constellationAnchorGroups = m_skyContextController->resolvedConstellationAnchorGroups()
            },
        .selectionEphemerisRequest = ephemerisRequestContext.request,
        .catalogSourceIds = m_skyContextController->catalogSourceIds(),
        .catalogSourceContributors = &m_skyContextController->catalogSourceContributorIds(),
        .catalogSourceTitles = m_skyContextController->catalogSourceTitles(),
        .timeController = m_skyContextController->timeController(),
        .selectedObjectTargetId = m_inspectorPlacement.targetId(),
        .selectedSearchTargetKind = m_skyContextController->selectedSearchTargetKind(),
        .selectedSearchTargetId = m_skyContextController->selectedSearchTargetId(),
        .trackedTargetKind = m_skyContextController->trackedTargetKind(),
        .trackedTargetId = m_skyContextController->trackedTargetId(),
        .inspectorPinnedX = m_inspectorPlacement.pinnedX(),
        .inspectorPinnedY = m_inspectorPlacement.pinnedY(),
        .inspectorPinned = m_inspectorPlacement.pinned(),
        .viewportWidth = m_viewportWidth,
        .viewportHeight = m_viewportHeight,
        .guidanceStrategy = m_skyContextController->ephemerisGuidanceStrategy()
    };
}

void SkySceneModel::rebuildSceneFrame()
{
    skygate::ui::SkyPerformanceStageTimer timer;

    const auto input = buildSceneInput();
    const double inputMs = timer.stageMilliseconds();
    if (!input.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "scene rebuild skipped no-input elapsedMs=" << timer.elapsedMilliseconds();
        }
        return;
    }

    const auto frameResult = m_framePipeline.rebuild(input->frameInput, m_viewportWidth, m_viewportHeight);
    const double pipelineMs = timer.stageMilliseconds();
    if (!frameResult.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "scene rebuild skipped no-frame elapsedMs=" << timer.elapsedMilliseconds();
        }
        return;
    }

    const auto requestContext = m_skyContextController->ephemerisRequestContext();
    m_sceneFrame.preparedProjection = *frameResult->preparedProjection;
    m_sceneFrame.snapshot = frameResult->snapshot;
    const bool degradationChanged = m_degradationReporter.update(requestContext, input->frameInput.ephemerisEngine);

    const SkySceneCompositionResult compositionResult = m_sceneComposer.rebuild(m_sceneFrame, *input, *frameResult);
    const double compositionMs = timer.stageMilliseconds();
    if (!compositionResult.changed) {
        if (degradationChanged) {
            emit sceneFrameChanged();
        }
        return;
    }

    if (compositionResult.frameContentChanged) {
        m_hitTargetIndex.rebuild(m_sceneFrame.frame, *frameResult->snapshot);
    }
    const double hitIndexMs = timer.stageMilliseconds();
    m_overlayItems = m_sceneOverlayAdapter.overlayItems(m_sceneFrame.overlayItems);
    m_selectionMarker = m_sceneOverlayAdapter.selectionMarker(m_sceneFrame.selectionMarker);
    m_selectedObjectInspector = m_sceneOverlayAdapter.selectedObjectInspector(m_sceneFrame.selectedObjectInspector);
    const double adapterMs = timer.stageMilliseconds();
    publishViewportSnapshot(frameResult->renderFrameGeneration, compositionResult.trailTargetBodyIndex);
    emit sceneFrameChanged();

    if (skygate::ui::SkyPerformanceLogging::enabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "scene rebuild elapsedMs=" << timer.elapsedMilliseconds() << "inputMs=" << inputMs
            << "pipelineMs=" << pipelineMs << "compositionMs=" << compositionMs << "hitIndexMs=" << hitIndexMs
            << "adapterMs=" << adapterMs << "pipelineUpdated=" << frameResult->updated
            << "frameContentChanged=" << compositionResult.frameContentChanged
            << "points=" << static_cast<qsizetype>(m_sceneFrame.frame.points.size())
            << "lines=" << static_cast<qsizetype>(m_sceneFrame.frame.lines.size())
            << "glyphs=" << static_cast<qsizetype>(m_sceneFrame.frame.glyphs.size())
            << "labels=" << static_cast<qsizetype>(m_sceneFrame.frame.labels.size())
            << "overlays=" << static_cast<qsizetype>(m_sceneFrame.overlayItems.size());
    }
}
