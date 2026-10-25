#include "SkyContextController.hpp"
#include "SkyContextProjectionTypeCodec.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "math/NumericTolerance.hpp"
#include "math/ViewportMath.hpp"
#include "time/SkyQtTimeCodec.hpp"
#include "time/SkyTimeController.hpp"

#include <chrono>
#include <cmath>

using namespace skygate::ui::internal;

namespace {

constexpr int kLiveRecomputeThrottleIntervalMs = 10000;
// Qt wheel deltas count eighths of a degree; 120 units is one detent.
constexpr double kWheelAngleDeltaPerStep = 120.0;
constexpr double kWheelZoomStepScale = 0.90;

}  // namespace

skygate::core::UtcTimePoint SkyContextController::currentUtcTime() const
{
    return m_timeSource->nowUtc();
}

void SkyContextController::setLive(bool live)
{
    if (!m_timeline.setLive(live)) {
        return;
    }

    m_lastLiveRecomputeUtc.reset();
    if (m_timeline.live()) {
        m_liveClock.start(m_location.utcTime());
        const skygate::core::UtcTimePoint currentUtc = currentUtcTime();
        const skygate::core::UtcTimePoint timelineUtc = m_location.utcTime();
        m_timeline.setCatchingUpToCurrentUtc(timelineUtc < currentUtc);
    } else {
        m_liveClock.stop();
    }

    emit liveChanged();
}

bool SkyContextController::liveRecomputeThrottleApplies() const
{
    const auto* engine = ephemerisEngine();
    return engine != nullptr && skygate::ephemeris::EphemerisComputationPolicy::liveRecomputeThrottleApplies(*engine);
}

bool SkyContextController::liveRecomputeThrottled(const skygate::core::UtcTimePoint& nextUtc) const
{
    if (!liveRecomputeThrottleApplies() || !m_lastLiveRecomputeUtc.has_value()) {
        return false;
    }

    const auto elapsedTimelineTime = nextUtc - m_lastLiveRecomputeUtc.value();
    return std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTimelineTime).count())
           < kLiveRecomputeThrottleIntervalMs;
}

void SkyContextController::markLiveRecomputeTick(const skygate::core::UtcTimePoint& nextUtc)
{
    if (liveRecomputeThrottleApplies()) {
        m_lastLiveRecomputeUtc = nextUtc;
    }
}

bool SkyContextController::timelineToolbarCollapsed() const noexcept
{
    return m_timeline.toolbarCollapsed();
}

void SkyContextController::setTimelineToolbarCollapsed(const bool timelineToolbarCollapsed)
{
    if (!m_timeline.setToolbarCollapsed(timelineToolbarCollapsed)) {
        return;
    }

    emit timelineToolbarCollapsedChanged();
}

void SkyContextController::togglePlayPause()
{
    setLive(!m_timeline.live());
}

void SkyContextController::setSpeedMultiplier(const double speedMultiplier)
{
    if (!std::isfinite(speedMultiplier) || speedMultiplier <= 0.0) {
        emit speedMultiplierChanged();
        return;
    }

    if (!m_timeline.setSpeedMultiplier(speedMultiplier)) {
        return;
    }

    emit speedMultiplierChanged();
}

void SkyContextController::setStepSeconds(const int stepSeconds)
{
    if (stepSeconds <= 0) {
        emit stepSecondsChanged();
        return;
    }

    if (!m_timeline.setStepSeconds(stepSeconds)) {
        return;
    }

    emit stepSecondsChanged();
}

void SkyContextController::setMagnitudeCutoff(const double magnitudeCutoff)
{
    if (!std::isfinite(magnitudeCutoff)) {
        emit magnitudeCutoffChanged();
        return;
    }

    if (!m_view.setMagnitudeCutoff(magnitudeCutoff)) {
        return;
    }

    emit magnitudeCutoffChanged();
    emit skyContextChanged();
}

void SkyContextController::setViewCenter(const double altitudeDeg, const double azimuthDeg)
{
    if (hasTrackedTarget() && recenterTrackedTarget()) {
        return;
    }

    static_cast<void>(setViewCenterInternal(altitudeDeg, azimuthDeg));
}

bool SkyContextController::setViewCenterInternal(const double altitudeDeg, const double azimuthDeg)
{
    if (!std::isfinite(altitudeDeg) || !std::isfinite(azimuthDeg)) {
        emit viewDirectionChanged();
        return false;
    }

    if (!m_view.setCenter(altitudeDeg, azimuthDeg)) {
        return false;
    }

    emit viewDirectionChanged();
    emit skyContextChanged();
    return true;
}

void SkyContextController::panViewBy(const double deltaAzimuthDeg, const double deltaAltitudeDeg)
{
    if (!std::isfinite(deltaAzimuthDeg) || !std::isfinite(deltaAltitudeDeg)) {
        emit viewDirectionChanged();
        return;
    }

    setViewCenter(m_view.centerAltitudeDeg() + deltaAltitudeDeg, m_view.centerAzimuthDeg() + deltaAzimuthDeg);
}

void SkyContextController::zoomViewByWheelDelta(const int wheelDeltaY)
{
    if (wheelDeltaY == 0) {
        return;
    }

    const double wheelSteps = static_cast<double>(wheelDeltaY) / kWheelAngleDeltaPerStep;
    const double zoomMultiplier = std::pow(kWheelZoomStepScale, wheelSteps);
    setViewFieldOfViewDeg(m_view.fieldOfViewDeg() * zoomMultiplier);
}

void SkyContextController::zoomViewByScaleDelta(const double scaleDelta)
{
    if (!std::isfinite(scaleDelta) || scaleDelta <= 0.0
        || std::abs(scaleDelta - 1.0) < skygate::core::NumericTolerance::kDisplayChange) {
        return;
    }

    setViewFieldOfViewDeg(m_view.fieldOfViewDeg() / scaleDelta);
}

void SkyContextController::resetViewDirection()
{
    setViewCenter(
        skygate::core::ViewportMath::kDefaultCenterAltitudeDeg, skygate::core::ViewportMath::kDefaultCenterAzimuthDeg
    );
}

void SkyContextController::goLiveNow()
{
    setCurrentUtcTime(currentUtcTime());
    m_timeline.setCatchingUpToCurrentUtc(false);
    setLive(true);
}

void SkyContextController::stepForward()
{
    setLive(false);
    stepBySeconds(m_timeline.stepSeconds());
}

void SkyContextController::stepBackward()
{
    setLive(false);
    stepBySeconds(-m_timeline.stepSeconds());
}

void SkyContextController::setProjectionTypeText(const QString& projectionTypeText)
{
    const auto parsedType = SkyContextProjectionTypeCodec::fromString(projectionTypeText);
    if (!parsedType.has_value()) {
        emit projectionTypeChanged();
        return;
    }

    setProjectionType(parsedType.value());
}

void SkyContextController::tickUtcTime()
{
    if (!m_timeline.live()) {
        return;
    }

    const skygate::core::UtcTimePoint timelineUtc = m_location.utcTime();
    const skygate::core::UtcTimePoint clockUtc = m_liveClock.currentUtc();
    const skygate::core::UtcTimePoint wallUtc = currentUtcTime();
    const auto advancement = m_timeline.computeCatchUpAdvancement(timelineUtc, clockUtc, wallUtc);
    if (!advancement.has_value()) {
        return;
    }

    const skygate::core::UtcTimePoint& nextUtc = advancement->nextUtc;
    m_timeController->setUtcTimePoint(nextUtc);
    if (liveRecomputeThrottled(nextUtc)) {
        return;
    }

    m_timeline.setCatchingUpToCurrentUtc(advancement->catchingUpToCurrentUtc);
    setCurrentUtcTime(nextUtc);
    if (advancement->advancedWhileCatchingUp) {
        m_liveClock.resetAnchor(nextUtc);
    }
    markLiveRecomputeTick(nextUtc);
}

void SkyContextController::stepBySeconds(const int stepSeconds)
{
    if (stepSeconds == 0) {
        return;
    }

    setCurrentUtcTime(m_location.utcTime() + std::chrono::seconds(stepSeconds));
}

void SkyContextController::setCurrentUtc(const QDateTime& utcTime)
{
    setCurrentUtcTime(SkyQtTimeCodec::toUtcTimePoint(utcTime.toUTC()));
}

void SkyContextController::setCurrentUtcTime(const skygate::core::UtcTimePoint& utcTime)
{
    if (m_location.utcTime() == utcTime) {
        return;
    }

    m_location.setUtcTime(utcTime);
    m_timeController->setUtcTimePoint(utcTime);
    emit nightConditionsChanged();
    if (!recenterTrackedTarget(true)) {
        emit skyContextChanged();
    }
}

void SkyContextController::setViewFieldOfViewDeg(const double viewFieldOfViewDeg)
{
    if (!std::isfinite(viewFieldOfViewDeg)) {
        return;
    }

    if (!m_view.setFieldOfViewDeg(viewFieldOfViewDeg)) {
        return;
    }

    emit viewDirectionChanged();
    emit skyContextChanged();
}

void SkyContextController::setProjectionType(const skygate::core::ProjectionType projectionType)
{
    if (m_view.projectionType() == projectionType) {
        return;
    }

    if (!m_view.setProjectionType(projectionType)) {
        emit projectionTypeChanged();
        return;
    }

    emit projectionTypeChanged();
    emit viewDirectionChanged();
    emit skyContextChanged();
}
