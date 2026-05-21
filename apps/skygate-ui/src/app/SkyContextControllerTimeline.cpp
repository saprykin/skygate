#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkyTimeController.hpp"

#include <QCoreApplication>

#include "skygate/core/math/ViewportMath.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QLocationPermission>
#endif

using namespace skygate::ui::internal;

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
    return activeEphemerisEngineKind() == skygate::ephemeris::EphemerisEngineKind::HighPrecision;
}

bool SkyContextController::liveRecomputeThrottled(const skygate::core::UtcTimePoint& nextUtc) const
{
    if (!liveRecomputeThrottleApplies() || !m_lastLiveRecomputeUtc.has_value()) {
        return false;
    }

    const auto elapsedTimelineTime = nextUtc - m_lastLiveRecomputeUtc.value();
    return std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTimelineTime).count())
           < SkyContextControllerConstants::kThrottledLiveRecomputeIntervalMs;
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

    const double wheelSteps = static_cast<double>(wheelDeltaY) / SkyContextControllerConstants::kWheelAngleDeltaStep;
    const double zoomMultiplier = std::pow(SkyContextControllerConstants::kWheelZoomStepScale, wheelSteps);
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
    m_timeline.resetSpeedProgress();
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
        const double elapsedSeconds = std::chrono::duration<double>(nextUtc - timelineUtc).count();
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
        if (!m_timeline.catchingUpToCurrentUtc()) {
            m_timeline.resetSpeedProgress();
        }

        setCurrentUtcTime(nextUtc);
        m_liveClock.resetAnchor(nextUtc);
        markLiveRecomputeTick(nextUtc);
        return;
    }

    m_timeline.setCatchingUpToCurrentUtc(false);
    m_timeline.resetSpeedProgress();
    setCurrentUtcTime(nextUtc);
    markLiveRecomputeTick(nextUtc);
}

void SkyContextController::stepBySeconds(const int stepSeconds)
{
    if (stepSeconds == 0) {
        return;
    }

    if (!m_timeline.live()) {
        m_timeline.resetSpeedProgress();
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

    initializeCurrentLocation();
}

void SkyContextController::initializeCurrentLocation()
{
    if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
        updateLocationStatusText();
        return;
    }

#if SKYGATE_HAS_POSITIONING
    setLocationStatusText("Location: Initializing");

    if (m_location.positionSource() == nullptr) {
        m_location.setPositionSource(QGeoPositionInfoSource::createDefaultSource(this));
    }
    if (m_location.positionSource() == nullptr) {
        setLocationStatusText("Location: Device source unavailable");
        return;
    }

    if (!m_location.positionSourceConnected()) {
        connect(
            m_location.positionSource(),
            &QGeoPositionInfoSource::positionUpdated,
            this,
            &SkyContextController::applyCurrentLocation
        );
        connect(
            m_location.positionSource(),
            &QGeoPositionInfoSource::errorOccurred,
            this,
            [this](QGeoPositionInfoSource::Error) {
                if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
                    return;
                }

                setLocationStatusText("Location: Update failed");
            }
        );
        m_location.setPositionSourceConnected(true);
    }

    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Precise);
    permission.setAvailability(QLocationPermission::WhenInUse);

    auto startLocationUpdate = [this] {
        setLocationStatusText("Location: Locating");
        m_location.positionSource()->requestUpdate(SkyContextControllerConstants::kLocationUpdateTimeoutMs);
    };

    if (!m_location.requestLocationPermission()) {
        startLocationUpdate();
        return;
    }

    QCoreApplication* app = QCoreApplication::instance();
    if (app == nullptr) {
        setLocationStatusText("Location: Positioning unavailable");
        return;
    }

    const Qt::PermissionStatus permissionStatus = app->checkPermission(permission);
    if (permissionStatus == Qt::PermissionStatus::Granted) {
        startLocationUpdate();
        return;
    }

    if (permissionStatus == Qt::PermissionStatus::Undetermined) {
        setLocationStatusText("Location: Waiting for permission");
        app->requestPermission(permission, this, [this, startLocationUpdate](const QPermission& requestPermission) {
            if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
                return;
            }

            if (requestPermission.status() == Qt::PermissionStatus::Granted) {
                startLocationUpdate();
                return;
            }

            setLocationStatusText("Location: Permission denied");
        });
        return;
    }

    setLocationStatusText("Location: Permission denied");
#else
    setLocationStatusText("Location: Positioning unavailable");
#endif
}

void SkyContextController::applyCurrentLocation(const QGeoPositionInfo& positionInfo)
{
#if SKYGATE_HAS_POSITIONING
    if (m_location.source() != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    if (!positionInfo.isValid()) {
        return;
    }

    const QGeoCoordinate coordinate = positionInfo.coordinate();
    if (!coordinate.isValid()) {
        return;
    }

    skygate::core::GeoLocation nextObserver = m_location.observer();
    nextObserver.latitudeDeg = coordinate.latitude();
    nextObserver.longitudeDeg = coordinate.longitude();
    if (std::isfinite(coordinate.altitude())) {
        nextObserver.elevationMeters = coordinate.altitude();
    }

    if (!nextObserver.isValid()) {
        return;
    }

    applyObserverLocation(nextObserver);
    setLocationStatusText("Location: Using current location");
#else
    (void)positionInfo;
#endif
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
