#include "ConstellationReferenceCalculator.hpp"
#include "HorizontalCoordinate.hpp"
#include "SkyContextController.hpp"
#include "StringUtilities.hpp"
#include "diagnostics/SkyPerformanceLogging.hpp"
#include "diagnostics/SkyPerformanceStageTimer.hpp"
#include "selection/SkySelectionTargetKind.hpp"

#include <string>

using skygate::ephemeris::StringUtilities;

namespace {

const skygate::ephemeris::CelestialBodyState*
findBodyStateById(const skygate::ephemeris::EphemerisSnapshot& snapshot, const QString& targetId)
{
    const std::string normalizedTargetId = StringUtilities::normalizedLookupKey(targetId.toStdString());
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (StringUtilities::normalizedLookupKey(body.id) == normalizedTargetId) {
            return &state;
        }
    }

    return nullptr;
}

}  // namespace

bool SkyContextController::searchToolbarCollapsed() const noexcept
{
    return m_search.toolbarCollapsed();
}

void SkyContextController::setSearchToolbarCollapsed(const bool searchToolbarCollapsed)
{
    if (!m_search.setToolbarCollapsed(searchToolbarCollapsed)) {
        return;
    }

    if (m_search.toolbarCollapsed()) {
        clearSelectedSearchTarget();
    }
    emit searchToolbarCollapsedChanged();
}

bool SkyContextController::focusSearchTarget(const QString& targetKind, const QString& targetId)
{
    skygate::ui::SkyPerformanceStageTimer timer;

    const auto* engine = ephemerisEngine();
    if (engine == nullptr || targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return false;
    }

    const auto requestContext = ephemerisRequestContext();
    const double requestContextMs = timer.stageMilliseconds();
    if (skygate::ui::internal::SkySelectionTargetKind::isBody(targetKind)) {
        const std::string trimmedTargetId = targetId.trimmed().toStdString();
        const auto bodyState = engine->computeBodyState(requestContext.request, trimmedTargetId);
        const double lookupMs = timer.stageMilliseconds();
        if (!bodyState.has_value() || !bodyState->horizontal.isFinite()) {
            return false;
        }

        if (hasTrackedTarget()
            && (!skygate::ui::internal::SkySelectionTargetKind::isBody(m_search.trackedTargetKind())
                || StringUtilities::normalizedLookupKey(m_search.trackedTargetId().toStdString())
                       != StringUtilities::normalizedLookupKey(targetId.toStdString()))) {
            clearTrackedTarget();
        }
        const double trackingMs = timer.stageMilliseconds();

        setSelectedSearchTarget(
            skygate::ui::internal::SkySelectionTargetKind::token(
                skygate::ui::internal::SkySelectionTargetKind::Type::Body
            ),
            targetId
        );
        const double selectedMs = timer.stageMilliseconds();
        setViewCenter(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
        const double centerMs = timer.stageMilliseconds();
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << timer.elapsedMilliseconds() << "requestContextMs=" << requestContextMs
                << "bodyLookupMs=" << lookupMs << "trackingMs=" << trackingMs << "selectedSignalMs=" << selectedMs
                << "viewCenterMs=" << centerMs << "kind=" << targetKind << "id=" << targetId;
        }
        return true;
    }

    if (skygate::ui::internal::SkySelectionTargetKind::isConstellationLabel(targetKind)) {
        const auto snapshot = engine->compute(requestContext.request);
        const double snapshotMs = timer.stageMilliseconds();
        const auto center = skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(
            snapshot, resolvedConstellationAnchorGroups(), targetId.toStdString()
        );
        const double centerLookupMs = timer.stageMilliseconds();
        if (!center.has_value()) {
            return false;
        }

        if (hasTrackedTarget()) {
            clearTrackedTarget();
        }
        const double trackingMs = timer.stageMilliseconds();

        setSelectedSearchTarget(
            skygate::ui::internal::SkySelectionTargetKind::token(
                skygate::ui::internal::SkySelectionTargetKind::Type::ConstellationLabel
            ),
            targetId
        );
        const double selectedMs = timer.stageMilliseconds();
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        const double centerMs = timer.stageMilliseconds();
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << timer.elapsedMilliseconds() << "requestContextMs=" << requestContextMs
                << "snapshotMs=" << snapshotMs << "labelLookupMs=" << centerLookupMs << "trackingMs=" << trackingMs
                << "selectedSignalMs=" << selectedMs << "viewCenterMs=" << centerMs << "kind=" << targetKind
                << "id=" << targetId;
        }
        return true;
    }

    return false;
}

bool SkyContextController::trackSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (engine == nullptr || !skygate::ui::internal::SkySelectionTargetKind::isBody(targetKind)
        || targetId.trimmed().isEmpty()) {
        return false;
    }

    skygate::core::ObservationContext trackingContext = m_location.context();
    const auto requestContext = ephemerisRequestContextFor(trackingContext);
    const auto snapshot = engine->compute(requestContext.request);
    const auto* bodyState = findBodyStateById(snapshot, targetId);
    if (bodyState == nullptr || !bodyState->horizontal.isFinite()) {
        return false;
    }

    const auto& body = snapshot.bodyAt(bodyState->bodyIndex);
    const QString displayText =
        !body.displayName.empty() ? QString::fromStdString(body.displayName) : targetId.trimmed();
    const bool shouldEmitLiveChanged = !m_timeline.live();

    m_timeline.startLiveAtCurrentUtc();
    m_timeline.setCatchingUpToCurrentUtc(m_location.utcTime() < currentUtcTime());
    m_liveClock.start(m_location.utcTime());
    if (shouldEmitLiveChanged) {
        emit liveChanged();
    }

    const QString bodyTargetKind =
        skygate::ui::internal::SkySelectionTargetKind::token(skygate::ui::internal::SkySelectionTargetKind::Type::Body);
    setTrackedTarget(bodyTargetKind, targetId, displayText);
    setSelectedSearchTarget(bodyTargetKind, targetId);
    const bool viewChanged = setViewCenterInternal(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
    if (!viewChanged) {
        emit skyContextChanged();
    }

    return true;
}

void SkyContextController::clearTrackedTarget()
{
    setTrackedTarget(QString(), QString(), QString());
}

bool SkyContextController::recenterTrackedTarget(const bool emitSkyContextChangedWhenUnchanged)
{
    if (!hasTrackedTarget()) {
        return false;
    }

    const auto* engine = ephemerisEngine();
    if (engine == nullptr || !skygate::ui::internal::SkySelectionTargetKind::isBody(m_search.trackedTargetKind())) {
        clearTrackedTarget();
        return false;
    }

    const auto requestContext = ephemerisRequestContext();
    const auto snapshot = engine->compute(requestContext.request);
    const auto* bodyState = findBodyStateById(snapshot, m_search.trackedTargetId());
    if (bodyState == nullptr || !bodyState->horizontal.isFinite()) {
        clearTrackedTarget();
        return false;
    }

    const auto& body = snapshot.bodyAt(bodyState->bodyIndex);
    if (!body.displayName.empty()) {
        setTrackedTarget(
            m_search.trackedTargetKind(), m_search.trackedTargetId(), QString::fromStdString(body.displayName)
        );
    }

    const bool viewChanged = setViewCenterInternal(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
    if (!viewChanged && emitSkyContextChangedWhenUnchanged) {
        emit skyContextChanged();
    }

    return true;
}
