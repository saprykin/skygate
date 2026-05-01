#include "SkyContextController.hpp"

#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"
#include "skygate/core/SystemTimeSource.hpp"

#include <cmath>

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

QDateTime currentUtcDateTime()
{
    const skygate::core::SystemTimeSource systemTimeSource;
    return skygate::ui::internal::SkyContextTimeCodec::toQDateTimeUtc(systemTimeSource.nowUtc());
}

const skygate::ephemeris::CelestialBodyState* findBodyStateById(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QString& targetId
)
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
    return m_search.toolbarCollapsed;
}

void SkyContextController::setSearchToolbarCollapsed(const bool searchToolbarCollapsed)
{
    if (m_search.toolbarCollapsed == searchToolbarCollapsed) {
        return;
    }

    m_search.toolbarCollapsed = searchToolbarCollapsed;
    if (m_search.toolbarCollapsed) {
        clearSelectedSearchTarget();
    }
    emit searchToolbarCollapsedChanged();
}

bool SkyContextController::focusSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (
        engine == nullptr
        || targetKind.trimmed().isEmpty()
        || targetId.trimmed().isEmpty()
    ) {
        return false;
    }

    const auto snapshot = engine->compute(m_location.context);
    const QString normalizedTargetKind = normalizedLookupKey(targetKind);
    if (normalizedTargetKind == "body") {
        const auto* bodyState = findBodyStateById(snapshot, targetId);
        if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
            return false;
        }

        if (
            hasTrackedTarget()
            && (
                normalizedLookupKey(m_search.trackedTargetKind) != "body"
                || normalizedLookupKey(m_search.trackedTargetId) != normalizedLookupKey(targetId)
            )
        ) {
            clearTrackedTarget();
        }

        setSelectedSearchTarget("body", targetId);
        setViewCenter(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
        return true;
    }

    if (normalizedTargetKind == "constellationlabel") {
        const auto center = skygate::ephemeris::ConstellationReferenceCalculator::labelCenter(
            snapshot,
            constellationLabelRefs(),
            targetId.toStdString()
        );
        if (!center.has_value()) {
            return false;
        }

        if (hasTrackedTarget()) {
            clearTrackedTarget();
        }

        setSelectedSearchTarget("constellationLabel", targetId);
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        return true;
    }

    return false;
}

bool SkyContextController::trackSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (
        engine == nullptr
        || normalizedLookupKey(targetKind) != "body"
        || targetId.trimmed().isEmpty()
    ) {
        return false;
    }

    const QDateTime currentUtc = currentUtcDateTime();
    skygate::core::SkyContext trackingContext = m_location.context;
    trackingContext.utcTime = skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(
        currentUtc.toUTC()
    );
    const auto snapshot = engine->compute(trackingContext);
    const auto* bodyState = findBodyStateById(snapshot, targetId);
    if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
        return false;
    }

    const auto& body = snapshot.bodyAt(bodyState->bodyIndex);
    const QString displayText = !body.displayName.empty()
        ? QString::fromStdString(body.displayName)
        : targetId.trimmed();
    const auto nextUtc = skygate::ui::internal::SkyContextTimeCodec::toUtcTimePoint(
        currentUtc.toUTC()
    );
    const bool utcChanged = m_location.context.utcTime != nextUtc;
    const bool shouldEmitLiveChanged = !m_timeline.live;

    m_location.context.utcTime = nextUtc;
    m_timeline.live = true;
    m_timeline.catchingUpToCurrentUtc = false;
    m_timeline.speedRemainderSeconds = 0.0;

    if (utcChanged) {
        emit utcDateTextChanged();
        emit utcTimeTextChanged();
    }
    if (shouldEmitLiveChanged) {
        emit liveChanged();
    }

    setTrackedTarget("body", targetId, displayText);
    setSelectedSearchTarget("body", targetId);
    const bool viewChanged = setViewCenterInternal(
        bodyState->horizontal.altitudeDeg,
        bodyState->horizontal.azimuthDeg
    );
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
    if (engine == nullptr || normalizedLookupKey(m_search.trackedTargetKind) != "body") {
        clearTrackedTarget();
        return false;
    }

    const auto snapshot = engine->compute(m_location.context);
    const auto* bodyState = findBodyStateById(snapshot, m_search.trackedTargetId);
    if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
        clearTrackedTarget();
        return false;
    }

    const auto& body = snapshot.bodyAt(bodyState->bodyIndex);
    if (!body.displayName.empty()) {
        setTrackedTarget(
            m_search.trackedTargetKind,
            m_search.trackedTargetId,
            QString::fromStdString(body.displayName)
        );
    }

    const bool viewChanged = setViewCenterInternal(
        bodyState->horizontal.altitudeDeg,
        bodyState->horizontal.azimuthDeg
    );
    if (!viewChanged && emitSkyContextChangedWhenUnchanged) {
        emit skyContextChanged();
    }

    return true;
}
