#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QTime>
#include <QTimeZone>

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/core/SystemTimeSource.hpp"
#include "skygate/core/math/ViewportMath.hpp"

#include <algorithm>
#include <cmath>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QLocationPermission>
#endif

using namespace skygate::ui::internal;

namespace {
QDateTime currentUtcDateTime()
{
    const skygate::core::SystemTimeSource systemTimeSource;
    return SkyContextTimeCodec::toQDateTimeUtc(systemTimeSource.nowUtc());
}
} // namespace

void SkyContextController::setLive(bool live)
{
    if (m_live == live) {
        return;
    }

    m_live = live;
    m_speedRemainderSeconds = 0.0;
    m_catchingUpToCurrentUtc = false;

    if (m_live) {
        const QDateTime currentUtc = currentUtcDateTime();
        const QDateTime timelineUtc = SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime);
        m_catchingUpToCurrentUtc = timelineUtc < currentUtc;
    }

    emit liveChanged();
}

bool SkyContextController::timelineToolbarCollapsed() const noexcept
{
    return m_timelineToolbarCollapsed;
}

void SkyContextController::setTimelineToolbarCollapsed(const bool timelineToolbarCollapsed)
{
    if (m_timelineToolbarCollapsed == timelineToolbarCollapsed) {
        return;
    }

    m_timelineToolbarCollapsed = timelineToolbarCollapsed;
    emit timelineToolbarCollapsedChanged();
}

void SkyContextController::togglePlayPause()
{
    setLive(!m_live);
}

void SkyContextController::setSpeedMultiplier(const double speedMultiplier)
{
    if (!std::isfinite(speedMultiplier) || speedMultiplier <= 0.0) {
        emit speedMultiplierChanged();
        return;
    }

    if (std::abs(m_speedMultiplier - speedMultiplier) < 1e-9) {
        return;
    }

    m_speedMultiplier = speedMultiplier;
    m_speedRemainderSeconds = 0.0;
    emit speedMultiplierChanged();
}

void SkyContextController::setStepSeconds(const int stepSeconds)
{
    if (stepSeconds <= 0) {
        emit stepSecondsChanged();
        return;
    }

    if (m_stepSeconds == stepSeconds) {
        return;
    }

    m_stepSeconds = stepSeconds;
    emit stepSecondsChanged();
}

void SkyContextController::setMagnitudeCutoff(const double magnitudeCutoff)
{
    if (!std::isfinite(magnitudeCutoff)) {
        emit magnitudeCutoffChanged();
        return;
    }

    const double clamped = std::clamp(
        magnitudeCutoff,
        SkyContextControllerConstants::kMagnitudeCutoffMin,
        SkyContextControllerConstants::kMagnitudeCutoffMax
    );
    if (std::abs(m_magnitudeCutoff - clamped) < 1e-9) {
        return;
    }

    m_magnitudeCutoff = clamped;
    emit magnitudeCutoffChanged();
    emit skyContextChanged();
}

void SkyContextController::setViewCenter(const double altitudeDeg, const double azimuthDeg)
{
    if (!std::isfinite(altitudeDeg) || !std::isfinite(azimuthDeg)) {
        emit viewDirectionChanged();
        return;
    }

    const double nextAltitudeDeg = skygate::core::ViewportMath::clampAltitudeDeg(altitudeDeg);
    const double nextAzimuthDeg = skygate::core::ViewportMath::normalizeAzimuthDeg(azimuthDeg);
    if (
        std::abs(m_viewCenterAltitudeDeg - nextAltitudeDeg) < 1e-9
        && std::abs(m_viewCenterAzimuthDeg - nextAzimuthDeg) < 1e-9
    ) {
        return;
    }

    m_viewCenterAltitudeDeg = nextAltitudeDeg;
    m_viewCenterAzimuthDeg = nextAzimuthDeg;
    emit viewDirectionChanged();
    emit skyContextChanged();
}

void SkyContextController::panViewBy(const double deltaAzimuthDeg, const double deltaAltitudeDeg)
{
    if (!std::isfinite(deltaAzimuthDeg) || !std::isfinite(deltaAltitudeDeg)) {
        emit viewDirectionChanged();
        return;
    }

    setViewCenter(
        m_viewCenterAltitudeDeg + deltaAltitudeDeg,
        m_viewCenterAzimuthDeg + deltaAzimuthDeg
    );
}

void SkyContextController::zoomViewByWheelDelta(const int wheelDeltaY)
{
    if (wheelDeltaY == 0) {
        return;
    }

    const double wheelSteps =
        static_cast<double>(wheelDeltaY) / SkyContextControllerConstants::kWheelAngleDeltaStep;
    const double zoomMultiplier = std::pow(
        SkyContextControllerConstants::kWheelZoomStepScale,
        wheelSteps
    );
    setViewFieldOfViewDeg(m_viewFieldOfViewDeg * zoomMultiplier);
}

void SkyContextController::zoomViewByScaleDelta(const double scaleDelta)
{
    if (!std::isfinite(scaleDelta) || scaleDelta <= 0.0 || std::abs(scaleDelta - 1.0) < 1e-9) {
        return;
    }

    setViewFieldOfViewDeg(m_viewFieldOfViewDeg / scaleDelta);
}

void SkyContextController::resetViewDirection()
{
    setViewCenter(
        skygate::core::ViewportMath::kDefaultCenterAltitudeDeg,
        skygate::core::ViewportMath::kDefaultCenterAzimuthDeg
    );
}

void SkyContextController::goLiveNow()
{
    setCurrentUtc(currentUtcDateTime());
    m_speedRemainderSeconds = 0.0;
    m_catchingUpToCurrentUtc = false;
    setLive(true);
}

void SkyContextController::stepForward()
{
    setLive(false);
    stepBySeconds(m_stepSeconds);
}

void SkyContextController::stepBackward()
{
    setLive(false);
    stepBySeconds(-m_stepSeconds);
}

bool SkyContextController::setUtcDateTimeText(
    const QString& utcDateText,
    const QString& utcTimeText
)
{
    const auto date = QDate::fromString(utcDateText.trimmed(), "yyyy-MM-dd");
    if (!date.isValid()) {
        return false;
    }

    const auto time = QTime::fromString(utcTimeText.trimmed(), "HH:mm:ss");
    if (!time.isValid()) {
        return false;
    }

    const QDateTime nextUtc(
        date,
        time,
        QTimeZone::UTC
    );

    setCurrentUtc(nextUtc);
    setLive(false);
    return true;
}

void SkyContextController::setLatitudeText(const QString& latitudeText)
{
    bool isValidNumber = false;
    const double latitude = latitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.latitudeDeg = latitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit latitudeTextChanged();
        return;
    }

    if (m_skyContext.observer.latitudeDeg == nextObserver.latitudeDeg) {
        return;
    }

    if (m_locationSource != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setLongitudeText(const QString& longitudeText)
{
    bool isValidNumber = false;
    const double longitude = longitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.longitudeDeg = longitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit longitudeTextChanged();
        return;
    }

    if (m_skyContext.observer.longitudeDeg == nextObserver.longitudeDeg) {
        return;
    }

    if (m_locationSource != SkyContextLocationSource::Custom) {
        setLocationSource(SkyContextLocationSource::Custom);
    }

    applyObserverLocation(nextObserver);
}

void SkyContextController::setElevationText(const QString& elevationText)
{
    bool isValidNumber = false;
    const double elevation = elevationText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.elevationMeters = elevation;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit elevationTextChanged();
        return;
    }

    if (m_skyContext.observer.elevationMeters == nextObserver.elevationMeters) {
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
    if (!m_live) {
        return;
    }

    const QDateTime currentUtc = currentUtcDateTime();
    const QDateTime timelineUtc = SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime);
    const bool catchingUpToCurrentUtc = m_catchingUpToCurrentUtc && timelineUtc < currentUtc;
    if (catchingUpToCurrentUtc) {
        m_speedRemainderSeconds += m_speedMultiplier * static_cast<double>(m_stepSeconds);
        const int wholeSeconds = static_cast<int>(std::floor(m_speedRemainderSeconds));
        if (wholeSeconds <= 0) {
            return;
        }

        m_speedRemainderSeconds -= wholeSeconds;
        int appliedStepSeconds = wholeSeconds;
        const qint64 secondsUntilCurrentUtc = timelineUtc.secsTo(currentUtc);
        appliedStepSeconds = std::min(
            appliedStepSeconds,
            static_cast<int>(secondsUntilCurrentUtc)
        );
        m_catchingUpToCurrentUtc = appliedStepSeconds < secondsUntilCurrentUtc;
        if (!m_catchingUpToCurrentUtc) {
            m_speedRemainderSeconds = 0.0;
        }

        stepBySeconds(appliedStepSeconds);
        return;
    }

    m_catchingUpToCurrentUtc = false;
    m_speedRemainderSeconds = 0.0;
    stepBySeconds(1);
}

void SkyContextController::stepBySeconds(const int stepSeconds)
{
    if (stepSeconds == 0) {
        return;
    }

    if (!m_live) {
        m_speedRemainderSeconds = 0.0;
    }

    setCurrentUtc(SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).addSecs(stepSeconds));
}

void SkyContextController::setCurrentUtc(const QDateTime& utcTime)
{
    const auto nextUtc = SkyContextTimeCodec::toUtcTimePoint(utcTime.toUTC());
    if (m_skyContext.utcTime == nextUtc) {
        return;
    }

    m_skyContext.utcTime = nextUtc;
    emit utcDateTextChanged();
    emit utcTimeTextChanged();
    emit skyContextChanged();
}

void SkyContextController::setViewFieldOfViewDeg(const double viewFieldOfViewDeg)
{
    if (!std::isfinite(viewFieldOfViewDeg)) {
        return;
    }

    const double nextViewFieldOfViewDeg =
        skygate::core::ViewportMath::clampFieldOfViewDeg(viewFieldOfViewDeg);
    if (std::abs(m_viewFieldOfViewDeg - nextViewFieldOfViewDeg) < 1e-9) {
        return;
    }

    m_viewFieldOfViewDeg = nextViewFieldOfViewDeg;
    emit viewDirectionChanged();
    emit skyContextChanged();
}

void SkyContextController::applyObserverLocation(const skygate::core::GeoLocation& observer)
{
    if (!observer.isValid()) {
        return;
    }

    const bool latitudeChanged = m_skyContext.observer.latitudeDeg != observer.latitudeDeg;
    const bool longitudeChanged = m_skyContext.observer.longitudeDeg != observer.longitudeDeg;
    const bool elevationChanged = m_skyContext.observer.elevationMeters != observer.elevationMeters;
    if (!latitudeChanged && !longitudeChanged && !elevationChanged) {
        return;
    }

    m_skyContext.observer = observer;
    if (latitudeChanged) {
        emit latitudeTextChanged();
    }
    if (longitudeChanged) {
        emit longitudeTextChanged();
    }
    if (elevationChanged) {
        emit elevationTextChanged();
    }
    emit skyContextChanged();
}

void SkyContextController::refreshCurrentLocation()
{
    if (m_locationSource != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    initializeCurrentLocation();
}

void SkyContextController::initializeCurrentLocation()
{
    if (m_locationSource != SkyContextLocationSource::CurrentDevice) {
        updateLocationStatusText();
        return;
    }

#if SKYGATE_HAS_POSITIONING
    setLocationStatusText("Location: Initializing");

    if (m_positionSource == nullptr) {
        m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    }
    if (m_positionSource == nullptr) {
        setLocationStatusText("Location: Device source unavailable");
        return;
    }

    if (!m_positionSourceConnected) {
        connect(
            m_positionSource,
            &QGeoPositionInfoSource::positionUpdated,
            this,
            &SkyContextController::applyCurrentLocation
        );
        connect(
            m_positionSource,
            &QGeoPositionInfoSource::errorOccurred,
            this,
            [this](QGeoPositionInfoSource::Error) {
                if (m_locationSource != SkyContextLocationSource::CurrentDevice) {
                    return;
                }

                setLocationStatusText("Location: Update failed");
            }
        );
        m_positionSourceConnected = true;
    }

    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Precise);
    permission.setAvailability(QLocationPermission::WhenInUse);

    auto startLocationUpdate = [this] {
        setLocationStatusText("Location: Locating");
        m_positionSource->requestUpdate(SkyContextControllerConstants::kLocationUpdateTimeoutMs);
    };

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
        app->requestPermission(
            permission,
            this,
            [this, startLocationUpdate](const QPermission& requestPermission) {
                if (m_locationSource != SkyContextLocationSource::CurrentDevice) {
                    return;
                }

                if (requestPermission.status() == Qt::PermissionStatus::Granted) {
                    startLocationUpdate();
                    return;
                }

                setLocationStatusText("Location: Permission denied");
            }
        );
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
    if (m_locationSource != SkyContextLocationSource::CurrentDevice) {
        return;
    }

    if (!positionInfo.isValid()) {
        return;
    }

    const QGeoCoordinate coordinate = positionInfo.coordinate();
    if (!coordinate.isValid()) {
        return;
    }

    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
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
    if (m_projectionType == projectionType) {
        return;
    }

    std::unique_ptr<skygate::core::IProjection> projection = skygate::core::createProjection(projectionType);
    if (projection == nullptr) {
        emit projectionTypeChanged();
        return;
    }

    m_projectionType = projectionType;
    m_projection = std::move(projection);
    emit projectionTypeChanged();
    emit viewDirectionChanged();
    emit skyContextChanged();
}
