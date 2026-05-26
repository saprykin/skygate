#include "SkySceneModel.hpp"

#include "SkyContextController.hpp"
#include "SkyPerformanceLogging.hpp"
#include "SkyQtTimeCodec.hpp"
#include "SkyTimeController.hpp"
#include "Types.hpp"

#include "engine/EphemerisPrecisionPolicy.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "time/CalendarTime.hpp"

#include <QElapsedTimer>
#include <QStringList>

#include <cmath>
#include <optional>

namespace {

[[nodiscard]] bool epochInRange(
    const skygate::ephemeris::AstronomicalEpoch& epoch, const skygate::ephemeris::EphemerisDateRange& range
) noexcept
{
    const double key = epoch.sortKey();
    return key >= range.start.sortKey() && key <= range.end.sortKey();
}

[[nodiscard]] QString formatEpochDate(const skygate::ephemeris::AstronomicalEpoch& epoch)
{
    const auto dateTime = skygate::ephemeris::CalendarTime::civilDateTimeFromAstronomicalEpoch(epoch);
    if (!dateTime.has_value()) {
        return QStringLiteral("--");
    }

    const int historicalYear =
        skygate::ephemeris::CalendarTime::historicalYearFromAstronomicalYear(dateTime->astronomicalYear);
    return SkyQtTimeCodec::formatDateText(QDate(historicalYear, dateTime->month, dateTime->day));
}

[[nodiscard]] QString rangeLabel(const skygate::ephemeris::EphemerisDateRange& range)
{
    const QString displayName = QString::fromStdString(range.displayName);
    const QString dateText = QStringLiteral("%1 to %2").arg(formatEpochDate(range.start), formatEpochDate(range.end));
    return displayName.isEmpty() ? dateText : QStringLiteral("%1: %2").arg(displayName, dateText);
}

void appendUniqueReason(QStringList& reasons, const QString& reason)
{
    if (!reasons.contains(reason)) {
        reasons.push_back(reason);
    }
}

[[nodiscard]] bool isKernelRange(const skygate::ephemeris::EphemerisDateRange& range)
{
    const QString id = QString::fromStdString(range.id).toLower();
    const QString displayName = QString::fromStdString(range.displayName).toLower();
    return id.contains(QStringLiteral("kernel")) || displayName.contains(QStringLiteral("kernel"));
}

void appendRangeWarning(
    QStringList& reasons,
    const skygate::ephemeris::AstronomicalEpoch& epoch,
    const std::optional<skygate::ephemeris::EphemerisDateRange>& range,
    const QString& label
)
{
    if (!range.has_value() || epochInRange(epoch, *range)) {
        return;
    }

    appendUniqueReason(reasons, QStringLiteral("%1 out of range. Supported range: %2.").arg(label, rangeLabel(*range)));
}

[[nodiscard]] QVariantList degradationReasonsForContext(
    const SkyContextController::EphemerisRequestContext& context, const skygate::ephemeris::IEphemerisEngine* engine
)
{
    QStringList reasons;
    if (engine != nullptr) {
        for (const skygate::ephemeris::EphemerisDateRange& range : engine->supportedDateRanges()) {
            if (isKernelRange(range) && !epochInRange(context.request.epoch, range)) {
                appendUniqueReason(
                    reasons,
                    QStringLiteral("Planetary kernel out of range. Supported range: %1.").arg(rangeLabel(range))
                );
            }
        }
    }

    appendRangeWarning(
        reasons, context.request.epoch, context.earthOrientationDataRange, QStringLiteral("Earth orientation data")
    );
    appendRangeWarning(
        reasons, context.request.epoch, context.leapSecondTableRange, QStringLiteral("Leap-second table")
    );
    appendRangeWarning(reasons, context.request.epoch, context.deltaTDataRange, QStringLiteral("Delta T data"));

    QVariantList result;
    result.reserve(reasons.size());
    for (const QString& reason : reasons) {
        result.push_back(reason);
    }
    return result;
}

}  // namespace

SkySceneModel::SkySceneModel(QObject* parent) : QObject(parent) {}

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
            m_skyContextController, &SkyContextController::skyContextChanged, this, &SkySceneModel::rebuildSceneFrame
        );
        m_selectedSearchTargetChangedConnection =
            connect(m_skyContextController, &SkyContextController::selectedSearchTargetChanged, this, [this] {
                if (m_ignoringSearchTargetChange) {
                    return;
                }
                m_selectedObjectTargetId.clear();
                m_selectedObjectInspectorPinned = false;
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

    emit skyContextControllerChanged();
    rebuildSceneFrame();
}

QVariantList SkySceneModel::overlayItems() const
{
    return m_overlayItems;
}

QVariantList SkySceneModel::ephemerisDegradationReasons() const
{
    return m_ephemerisDegradationReasons;
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

void SkySceneModel::setViewportSize(const double viewportWidth, const double viewportHeight)
{
    if (std::abs(m_viewportWidth - viewportWidth) < 1e-9 && std::abs(m_viewportHeight - viewportHeight) < 1e-9) {
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
    m_selectedObjectTargetId = QString::fromStdString(body.id);
    m_selectedObjectInspectorPinned = false;
    m_sceneComposer.reset();
    rebuildSceneFrame();
    return true;
}

void SkySceneModel::clearSelectedObjectInspector()
{
    const bool hadSelection = !m_selectedObjectTargetId.isEmpty() || m_selectedObjectInspectorPinned
                              || m_sceneFrame.selectedObjectInspector.visible;
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
        if (!m_sceneFrame.selectedObjectInspector.visible) {
            return;
        }

        m_selectedObjectInspectorPinnedX = m_sceneFrame.selectedObjectInspector.x;
        m_selectedObjectInspectorPinnedY = m_sceneFrame.selectedObjectInspector.y;
        m_selectedObjectInspectorPinned = true;
        m_sceneFrame.selectedObjectInspector.pinned = true;
        m_selectedObjectInspector = m_sceneOverlayAdapter.selectedObjectInspector(m_sceneFrame.selectedObjectInspector);
        emit sceneFrameChanged();
        return;
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
    if (m_sceneFrame.selectedObjectInspector.visible) {
        m_sceneFrame.selectedObjectInspector.x = x;
        m_sceneFrame.selectedObjectInspector.y = y;
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

std::optional<skygate::core::SkyContext> SkySceneModel::referenceOverlayContext() const
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
    const bool hadSceneFrame = m_framePipeline.clear() || m_sceneFrame.preparedProjection.has_value()
                               || m_sceneFrame.snapshot != nullptr || !m_sceneFrame.frame.points.empty()
                               || !m_sceneFrame.frame.lines.empty() || !m_sceneFrame.frame.glyphs.empty()
                               || !m_sceneFrame.overlayItems.empty() || m_sceneFrame.selectionMarker.visible
                               || m_sceneFrame.selectedObjectInspector.visible || !m_overlayItems.isEmpty()
                               || !m_ephemerisDegradationReasons.isEmpty() || !m_selectionMarker.isEmpty()
                               || !m_selectedObjectInspector.isEmpty();
    m_hitTargetIndex.clear();
    m_sceneComposer.reset();
    m_sceneFrame = {};
    m_overlayItems = {};
    m_ephemerisDegradationReasons = {};
    m_selectionMarker = {};
    m_selectedObjectInspector = {};
    return hadSceneFrame;
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
    const auto renderEphemerisRequest = skygate::ephemeris::ephemerisRequestForPrecisionPolicy(
        ephemerisRequestContext.request, skygate::ephemeris::EphemerisPrecisionPolicy::SceneRender
    );

    return SkySceneCompositionInput{
        .frameInput =
            SkySceneFramePipelineInput{
                .ephemerisEngine = ephemerisEngine,
                .skyContext = ephemerisRequestContext.request.context,
                .ephemerisRequest = renderEphemerisRequest,
                .catalogRevision = ephemerisRequestContext.catalogRevision,
                .engineKind = ephemerisRequestContext.request.options.engineKind,
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
                .constellationLineRefs = m_skyContextController->constellationLineRefs(),
                .constellationAnchorGroups = m_skyContextController->constellationAnchorGroups()
            },
        .selectionEphemerisRequest = ephemerisRequestContext.request,
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
    QElapsedTimer timer;
    skygate::ui::startPerformanceTimer(timer);

    const auto input = buildSceneInput();
    const qint64 inputNs = skygate::ui::performanceElapsedNanoseconds(timer);
    if (!input.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        if (skygate::ui::performanceLoggingEnabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "scene rebuild skipped no-input elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer);
        }
        return;
    }

    const auto frameResult = m_framePipeline.rebuild(input->frameInput, m_viewportWidth, m_viewportHeight);
    const qint64 pipelineNs = skygate::ui::performanceElapsedNanoseconds(timer);
    if (!frameResult.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        if (skygate::ui::performanceLoggingEnabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "scene rebuild skipped no-frame elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer);
        }
        return;
    }

    const auto requestContext = m_skyContextController->ephemerisRequestContext();
    m_sceneFrame.preparedProjection = *frameResult->preparedProjection;
    m_sceneFrame.snapshot = frameResult->snapshot;
    const QVariantList ephemerisDegradationReasons =
        degradationReasonsForContext(requestContext, input->frameInput.ephemerisEngine);

    const SkySceneCompositionResult compositionResult = m_sceneComposer.rebuild(m_sceneFrame, *input, *frameResult);
    const qint64 compositionNs = skygate::ui::performanceElapsedNanoseconds(timer);
    if (!compositionResult.changed) {
        if (m_ephemerisDegradationReasons != ephemerisDegradationReasons) {
            m_ephemerisDegradationReasons = ephemerisDegradationReasons;
            emit sceneFrameChanged();
        }
        return;
    }

    if (compositionResult.frameContentChanged) {
        m_hitTargetIndex.rebuild(m_sceneFrame.frame, *frameResult->snapshot);
    }
    const qint64 hitIndexNs = skygate::ui::performanceElapsedNanoseconds(timer);
    m_overlayItems = m_sceneOverlayAdapter.overlayItems(m_sceneFrame.overlayItems);
    m_ephemerisDegradationReasons = ephemerisDegradationReasons;
    m_selectionMarker = m_sceneOverlayAdapter.selectionMarker(m_sceneFrame.selectionMarker);
    m_selectedObjectInspector = m_sceneOverlayAdapter.selectedObjectInspector(m_sceneFrame.selectedObjectInspector);
    const qint64 adapterNs = skygate::ui::performanceElapsedNanoseconds(timer);
    emit sceneFrameChanged();

    if (skygate::ui::performanceLoggingEnabled()) {
        qCInfo(skygate::ui::skygatePerfLog)
            << "scene rebuild elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
            << "inputMs=" << skygate::ui::performanceMilliseconds(inputNs)
            << "pipelineMs=" << skygate::ui::performanceMilliseconds(pipelineNs - inputNs)
            << "compositionMs=" << skygate::ui::performanceMilliseconds(compositionNs - pipelineNs)
            << "hitIndexMs=" << skygate::ui::performanceMilliseconds(hitIndexNs - compositionNs)
            << "adapterMs=" << skygate::ui::performanceMilliseconds(adapterNs - hitIndexNs)
            << "pipelineUpdated=" << frameResult->updated
            << "frameContentChanged=" << compositionResult.frameContentChanged
            << "points=" << static_cast<qsizetype>(m_sceneFrame.frame.points.size())
            << "lines=" << static_cast<qsizetype>(m_sceneFrame.frame.lines.size())
            << "glyphs=" << static_cast<qsizetype>(m_sceneFrame.frame.glyphs.size())
            << "labels=" << static_cast<qsizetype>(m_sceneFrame.frame.labels.size())
            << "overlays=" << static_cast<qsizetype>(m_sceneFrame.overlayItems.size());
    }
}
