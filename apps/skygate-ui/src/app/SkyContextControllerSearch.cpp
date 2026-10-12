#include "ConstellationReferenceCalculator.hpp"
#include "SkyContextController.hpp"
#include "SkyPerformanceLogging.hpp"
#include "StringUtilities.hpp"

#include <QElapsedTimer>

#include <cmath>
#include <string>

using skygate::ephemeris::StringUtilities;

namespace {

bool hasFiniteHorizontal(const skygate::core::HorizontalCoordinate& horizontal)
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

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
    QElapsedTimer timer;
    skygate::ui::SkyPerformanceLogging::startTimer(timer);

    const auto* engine = ephemerisEngine();
    if (engine == nullptr || targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return false;
    }

    const auto requestContext = ephemerisRequestContext();
    const qint64 requestContextNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
    const std::string normalizedTargetKind = StringUtilities::normalizedLookupKey(targetKind.toStdString());
    if (normalizedTargetKind == "body") {
        const std::string trimmedTargetId = targetId.trimmed().toStdString();
        const auto bodyState = engine->computeBodyState(requestContext.request, trimmedTargetId);
        const qint64 lookupNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        if (!bodyState.has_value() || !hasFiniteHorizontal(bodyState->horizontal)) {
            return false;
        }

        if (hasTrackedTarget()
            && (StringUtilities::normalizedLookupKey(m_search.trackedTargetKind().toStdString()) != "body"
                || StringUtilities::normalizedLookupKey(m_search.trackedTargetId().toStdString())
                       != StringUtilities::normalizedLookupKey(targetId.toStdString()))) {
            clearTrackedTarget();
        }
        const qint64 trackingNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);

        setSelectedSearchTarget("body", targetId);
        const qint64 selectedNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        setViewCenter(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
        const qint64 centerNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << skygate::ui::SkyPerformanceLogging::elapsedMilliseconds(timer)
                << "requestContextMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(requestContextNs)
                << "bodyLookupMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(lookupNs - requestContextNs)
                << "trackingMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(trackingNs - lookupNs)
                << "selectedSignalMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(selectedNs - trackingNs)
                << "viewCenterMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(centerNs - selectedNs)
                << "kind=" << targetKind << "id=" << targetId;
        }
        return true;
    }

    if (normalizedTargetKind == "constellationlabel") {
        const auto snapshot = engine->compute(requestContext.request);
        const qint64 snapshotNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        const auto center = skygate::ephemeris::ConstellationReferenceCalculator::anchorCentroid(
            snapshot, resolvedConstellationAnchorGroups(), targetId.toStdString()
        );
        const qint64 centerLookupNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        if (!center.has_value()) {
            return false;
        }

        if (hasTrackedTarget()) {
            clearTrackedTarget();
        }
        const qint64 trackingNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);

        setSelectedSearchTarget("constellationLabel", targetId);
        const qint64 selectedNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        const qint64 centerNs = skygate::ui::SkyPerformanceLogging::elapsedNanoseconds(timer);
        if (skygate::ui::SkyPerformanceLogging::enabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << skygate::ui::SkyPerformanceLogging::elapsedMilliseconds(timer)
                << "requestContextMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(requestContextNs)
                << "snapshotMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(snapshotNs - requestContextNs)
                << "labelLookupMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(centerLookupNs - snapshotNs)
                << "trackingMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(trackingNs - centerLookupNs)
                << "selectedSignalMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(selectedNs - trackingNs)
                << "viewCenterMs=" << skygate::ui::SkyPerformanceLogging::milliseconds(centerNs - selectedNs)
                << "kind=" << targetKind << "id=" << targetId;
        }
        return true;
    }

    return false;
}

bool SkyContextController::trackSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (engine == nullptr || StringUtilities::normalizedLookupKey(targetKind.toStdString()) != "body"
        || targetId.trimmed().isEmpty()) {
        return false;
    }

    skygate::core::ObservationContext trackingContext = m_location.context();
    const auto requestContext = ephemerisRequestContextFor(trackingContext);
    const auto snapshot = engine->compute(requestContext.request);
    const auto* bodyState = findBodyStateById(snapshot, targetId);
    if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
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

    setTrackedTarget("body", targetId, displayText);
    setSelectedSearchTarget("body", targetId);
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
    if (engine == nullptr
        || StringUtilities::normalizedLookupKey(m_search.trackedTargetKind().toStdString()) != "body") {
        clearTrackedTarget();
        return false;
    }

    const auto requestContext = ephemerisRequestContext();
    const auto snapshot = engine->compute(requestContext.request);
    const auto* bodyState = findBodyStateById(snapshot, m_search.trackedTargetId());
    if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
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
