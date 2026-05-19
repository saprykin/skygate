#include "SkyContextController.hpp"

#include "SkyPerformanceLogging.hpp"
#include "SkyTimeController.hpp"

#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include <QElapsedTimer>

#include <cmath>
#include <string>

namespace {

QString normalizedLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

QString normalizedLookupKey(const std::string& value)
{
    return normalizedLookupKey(QString::fromStdString(value));
}

bool hasFiniteHorizontal(const skygate::core::HorizontalCoordinate& horizontal)
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

const skygate::ephemeris::CelestialBodyState*
findBodyStateById(const skygate::ephemeris::SkySnapshot& snapshot, const QString& targetId)
{
    const QString normalizedTargetId = normalizedLookupKey(targetId);
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (normalizedLookupKey(body.id) == normalizedTargetId) {
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
    skygate::ui::startPerformanceTimer(timer);

    const auto* engine = ephemerisEngine();
    if (engine == nullptr || targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return false;
    }

    const auto requestContext = ephemerisRequestContext();
    const qint64 requestContextNs = skygate::ui::performanceElapsedNanoseconds(timer);
    const QString normalizedTargetKind = normalizedLookupKey(targetKind);
    if (normalizedTargetKind == "body") {
        const std::string trimmedTargetId = targetId.trimmed().toStdString();
        const auto bodyState = engine->computeBodyState(requestContext.request, trimmedTargetId);
        const qint64 lookupNs = skygate::ui::performanceElapsedNanoseconds(timer);
        if (!bodyState.has_value() || !hasFiniteHorizontal(bodyState->horizontal)) {
            return false;
        }

        if (hasTrackedTarget()
            && (normalizedLookupKey(m_search.trackedTargetKind()) != "body"
                || normalizedLookupKey(m_search.trackedTargetId()) != normalizedLookupKey(targetId))) {
            clearTrackedTarget();
        }
        const qint64 trackingNs = skygate::ui::performanceElapsedNanoseconds(timer);

        setSelectedSearchTarget("body", targetId);
        const qint64 selectedNs = skygate::ui::performanceElapsedNanoseconds(timer);
        setViewCenter(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
        const qint64 centerNs = skygate::ui::performanceElapsedNanoseconds(timer);
        if (skygate::ui::performanceLoggingEnabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
                << "requestContextMs=" << skygate::ui::performanceMilliseconds(requestContextNs)
                << "bodyLookupMs=" << skygate::ui::performanceMilliseconds(lookupNs - requestContextNs)
                << "trackingMs=" << skygate::ui::performanceMilliseconds(trackingNs - lookupNs)
                << "selectedSignalMs=" << skygate::ui::performanceMilliseconds(selectedNs - trackingNs)
                << "viewCenterMs=" << skygate::ui::performanceMilliseconds(centerNs - selectedNs)
                << "kind=" << targetKind << "id=" << targetId;
        }
        return true;
    }

    if (normalizedTargetKind == "constellationlabel") {
        const auto snapshot = engine->compute(requestContext.request);
        const qint64 snapshotNs = skygate::ui::performanceElapsedNanoseconds(timer);
        const auto center = skygate::ephemeris::ConstellationReferenceCalculator::labelCenter(
            snapshot, constellationLabelRefs(), targetId.toStdString()
        );
        const qint64 centerLookupNs = skygate::ui::performanceElapsedNanoseconds(timer);
        if (!center.has_value()) {
            return false;
        }

        if (hasTrackedTarget()) {
            clearTrackedTarget();
        }
        const qint64 trackingNs = skygate::ui::performanceElapsedNanoseconds(timer);

        setSelectedSearchTarget("constellationLabel", targetId);
        const qint64 selectedNs = skygate::ui::performanceElapsedNanoseconds(timer);
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        const qint64 centerNs = skygate::ui::performanceElapsedNanoseconds(timer);
        if (skygate::ui::performanceLoggingEnabled()) {
            qCInfo(skygate::ui::skygatePerfLog)
                << "search focus elapsedMs=" << skygate::ui::performanceElapsedMilliseconds(timer)
                << "requestContextMs=" << skygate::ui::performanceMilliseconds(requestContextNs)
                << "snapshotMs=" << skygate::ui::performanceMilliseconds(snapshotNs - requestContextNs)
                << "labelLookupMs=" << skygate::ui::performanceMilliseconds(centerLookupNs - snapshotNs)
                << "trackingMs=" << skygate::ui::performanceMilliseconds(trackingNs - centerLookupNs)
                << "selectedSignalMs=" << skygate::ui::performanceMilliseconds(selectedNs - trackingNs)
                << "viewCenterMs=" << skygate::ui::performanceMilliseconds(centerNs - selectedNs)
                << "kind=" << targetKind << "id=" << targetId;
        }
        return true;
    }

    return false;
}

bool SkyContextController::trackSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (engine == nullptr || normalizedLookupKey(targetKind) != "body" || targetId.trimmed().isEmpty()) {
        return false;
    }

    const QDateTime currentUtc = currentUtcDateTime();
    skygate::core::SkyContext trackingContext = m_location.context();
    trackingContext.utcTime = skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(currentUtc.toUTC());
    const auto requestContext = ephemerisRequestContextFor(trackingContext);
    const auto snapshot = engine->compute(requestContext.request);
    const auto* bodyState = findBodyStateById(snapshot, targetId);
    if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
        return false;
    }

    const auto& body = snapshot.bodyAt(bodyState->bodyIndex);
    const QString displayText =
        !body.displayName.empty() ? QString::fromStdString(body.displayName) : targetId.trimmed();
    const auto nextUtc = skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(currentUtc.toUTC());
    const bool utcChanged = m_location.utcTime() != nextUtc;
    const bool shouldEmitLiveChanged = !m_timeline.live();

    m_location.setUtcTime(nextUtc);
    m_timeController->setUtcDateTime(currentUtc.toUTC());
    m_timeline.startLiveAtCurrentUtc();

    if (utcChanged) {
        emit utcDateTextChanged();
        emit utcTimeTextChanged();
    }
    if (shouldEmitLiveChanged) {
        emit liveChanged();
    }

    setTrackedTarget("body", targetId, displayText);
    setSelectedSearchTarget("body", targetId);
    const bool viewChanged = setViewCenterInternal(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
    if (utcChanged && !viewChanged) {
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
    if (engine == nullptr || normalizedLookupKey(m_search.trackedTargetKind()) != "body") {
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
