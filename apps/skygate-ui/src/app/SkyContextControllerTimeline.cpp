#include "SkyContextController.hpp"
#include "SkyContextLocationSource.hpp"
#include "SkyContextProjectionTypeCodec.hpp"
#include "SkyContextTimeCodec.hpp"
#include "SkyContextUtcDateTimeTextCodec.hpp"
#include "engine/EphemerisComputationPolicy.hpp"
#include "location/SkyDeviceLocationAcquisition.hpp"
#include "math/ViewportMath.hpp"
#include "time/SkyTimeController.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace skygate::ui::internal;

namespace {

constexpr int kLiveRecomputeThrottleIntervalMs = 10000;
// Qt wheel deltas count eighths of a degree; 120 units is one detent.
constexpr double kWheelAngleDeltaPerStep = 120.0;
constexpr double kWheelZoomStepScale = 0.90;

}  // namespace

QDateTime SkyContextController::currentUtcDateTime() const
{
    return SkyContextTimeCodec::toQDateTimeUtc(currentUtcTime());
}

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
    if (!std::isfinite(scaleDelta) || scaleDelta <= 0.0 || std::abs(scaleDelta - 1.0) < 1e-9) {
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

QString SkyContextController::validateUtcDateTimeText(const QString& utcDateText, const QString& utcTimeText) const
{
    return SkyContextUtcDateTimeTextCodec::parse(utcDateText, utcTimeText).errorText;
}

bool SkyContextController::setUtcDateTimeText(const QString& utcDateText, const QString& utcTimeText)
{
    const auto parseResult = SkyContextUtcDateTimeTextCodec::parse(utcDateText, utcTimeText);
    if (!parseResult.isValid()) {
        return false;
    }

    setCurrentUtc(parseResult.utcDateTime);
    setLive(false);
    return true;
}

void SkyContextController::setLatitudeText(const QString& latitudeText)
{
    bool isValidNumber = false;
    const double latitude = latitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.latitudeDeg = latitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit latitudeTextChanged();
        return;
    }

    if (m_location.observer().latitudeDeg == nextObserver.latitudeDeg) {
        return;
    }

    if (m_location.source() != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setLongitudeText(const QString& longitudeText)
{
    bool isValidNumber = false;
    const double longitude = longitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.longitudeDeg = longitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit longitudeTextChanged();
        return;
    }

    if (m_location.observer().longitudeDeg == nextObserver.longitudeDeg) {
        return;
    }

    if (m_location.source() != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setElevationText(const QString& elevationText)
{
    bool isValidNumber = false;
    const double elevation = elevationText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.elevationMeters = elevation;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit elevationTextChanged();
        return;
    }

    if (m_location.observer().elevationMeters == nextObserver.elevationMeters) {
        return;
    }

    applyObserverLocation(nextObserver);
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

    skygate::core::UtcTimePoint nextUtc = m_liveClock.currentUtc();
    const skygate::core::UtcTimePoint currentWallUtc = currentUtcTime();
    const skygate::core::UtcTimePoint timelineUtc = m_location.utcTime();
    const bool catchingUpToCurrentUtc = m_timeline.catchingUpToCurrentUtc() && timelineUtc < currentWallUtc;
    if (catchingUpToCurrentUtc) {
        const double elapsedSeconds = std::max(1.0, std::chrono::duration<double>(nextUtc - timelineUtc).count());
        const double timelineAdvanceSeconds =
            elapsedSeconds * m_timeline.speedMultiplier() * static_cast<double>(m_timeline.stepSeconds());
        if (timelineAdvanceSeconds <= 0.0) {
            return;
        }

        nextUtc = timelineUtc
                  + std::chrono::duration_cast<skygate::core::UtcTimePoint::duration>(
                      std::chrono::duration<double>(timelineAdvanceSeconds)
                  );
        if (nextUtc > currentWallUtc) {
            nextUtc = currentWallUtc;
        }
    }

    m_timeController->setUtcTimePoint(nextUtc);
    if (liveRecomputeThrottled(nextUtc)) {
        return;
    }

    if (catchingUpToCurrentUtc) {
        m_timeline.setCatchingUpToCurrentUtc(nextUtc < currentWallUtc);
        setCurrentUtcTime(nextUtc);
        m_liveClock.resetAnchor(nextUtc);
        markLiveRecomputeTick(nextUtc);
        return;
    }

    m_timeline.setCatchingUpToCurrentUtc(false);
    setCurrentUtcTime(nextUtc);
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
    setCurrentUtcTime(SkyContextTimeCodec::toUtcTimePoint(utcTime.toUTC()));
}

void SkyContextController::setCurrentUtcTime(const skygate::core::UtcTimePoint& utcTime)
{
    if (m_location.utcTime() == utcTime) {
        return;
    }

    m_location.setUtcTime(utcTime);
    m_timeController->setUtcTimePoint(utcTime);
    emit utcDateTextChanged();
    emit utcTimeTextChanged();
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

void SkyContextController::applyObserverLocation(const skygate::core::GeoLocation& observer)
{
    if (!observer.isValid()) {
        return;
    }

    const bool latitudeChanged = m_location.observer().latitudeDeg != observer.latitudeDeg;
    const bool longitudeChanged = m_location.observer().longitudeDeg != observer.longitudeDeg;
    const bool elevationChanged = m_location.observer().elevationMeters != observer.elevationMeters;
    if (!latitudeChanged && !longitudeChanged && !elevationChanged) {
        return;
    }

    m_location.setObserver(observer);
    if (latitudeChanged) {
        emit latitudeTextChanged();
    }
    if (longitudeChanged) {
        emit longitudeTextChanged();
    }
    if (elevationChanged) {
        emit elevationTextChanged();
    }
    emit nightConditionsChanged();
    if (!recenterTrackedTarget(true)) {
        emit skyContextChanged();
    }
}

void SkyContextController::refreshCurrentLocation()
{
    if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    m_deviceLocationAcquisition->start();
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
