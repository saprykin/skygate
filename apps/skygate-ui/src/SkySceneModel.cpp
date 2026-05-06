#include "SkySceneModel.hpp"

#include "SkyContextController.hpp"
#include "SkyTimeController.hpp"

#include <cmath>

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
    m_sceneComposer.reset();
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
                m_sceneComposer.reset();
                rebuildSceneFrame();
            }
        );
        m_trackedTargetChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::trackedTargetChanged,
            this,
            [this] {
                m_sceneComposer.reset();
                rebuildSceneFrame();
            }
        );
        m_themeChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::themeChanged,
            this,
            &SkySceneModel::rebuildSceneFrame
        );
        m_timeZoneChangedConnection = connect(
            m_skyContextController->timeController(),
            &SkyTimeController::timeZoneChanged,
            this,
            [this] {
                m_sceneComposer.reset();
                rebuildSceneFrame();
            }
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
    m_sceneComposer.reset();
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

    m_sceneComposer.reset();
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
    m_sceneComposer.reset();
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
    m_sceneComposer.reset();
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
    m_sceneComposer.reset();
    m_sceneFrame = {};
    return hadSceneFrame;
}

std::optional<SkySceneCompositionInput> SkySceneModel::buildSceneInput() const
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

    return SkySceneCompositionInput {
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
        .timeController = m_skyContextController->timeController(),
        .selectedObjectTargetId = m_selectedObjectTargetId,
        .selectedSearchTargetKind = m_skyContextController->selectedSearchTargetKind(),
        .selectedSearchTargetId = m_skyContextController->selectedSearchTargetId(),
        .trackedTargetKind = m_skyContextController->trackedTargetKind(),
        .trackedTargetId = m_skyContextController->trackedTargetId(),
        .inspectorPinnedX = m_selectedObjectInspectorPinnedX,
        .inspectorPinnedY = m_selectedObjectInspectorPinnedY,
        .inspectorPinned = m_selectedObjectInspectorPinned,
        .viewportWidth = m_viewportWidth,
        .viewportHeight = m_viewportHeight
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

    const SkySceneCompositionResult compositionResult =
        m_sceneComposer.rebuild(m_sceneFrame, *input, *frameResult);
    if (!compositionResult.changed) {
        return;
    }

    if (compositionResult.frameContentChanged) {
        m_hitTargetIndex.rebuild(m_sceneFrame.frame, *frameResult->snapshot);
    }
    emit sceneFrameChanged();
}
