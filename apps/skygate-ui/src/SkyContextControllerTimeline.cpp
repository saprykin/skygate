#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QTime>
#include <QTimeZone>

#include "skygate/core/ProjectionFactory.hpp"

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
    return QDateTime::currentDateTimeUtc().toUTC();
}
} // namespace

void SkyContextController::setLive(bool live)
{
    if (m_live == live) {
        return;
    }

    m_live = live;
    m_speedRemainderSeconds = 0.0;

    if (m_live && m_restoreUtcLockStateOnLiveResume) {
        const bool nextUtcDateLocked = m_restoreUtcDateLockedOnLiveResume;
        const bool nextUtcTimeLocked = m_restoreUtcTimeLockedOnLiveResume;
        m_restoreUtcLockStateOnLiveResume = false;
        m_restoreUtcDateLockedOnLiveResume = false;
        m_restoreUtcTimeLockedOnLiveResume = false;

        if (m_utcDateLocked != nextUtcDateLocked) {
            m_utcDateLocked = nextUtcDateLocked;
            emit utcDateLockedChanged();
        }
        if (m_utcTimeLocked != nextUtcTimeLocked) {
            m_utcTimeLocked = nextUtcTimeLocked;
            emit utcTimeLockedChanged();
        }
    }

    if (m_live && (m_utcDateLocked || m_utcTimeLocked)) {
        // Jump to current UTC as soon as playback resumes in lock mode.
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
    }

    emit liveChanged();
}

bool SkyContextController::utcDateLocked() const noexcept
{
    return m_utcDateLocked;
}

bool SkyContextController::utcTimeLocked() const noexcept
{
    return m_utcTimeLocked;
}

void SkyContextController::setUtcDateLocked(const bool utcDateLocked)
{
    m_restoreUtcLockStateOnLiveResume = false;

    if (m_utcDateLocked == utcDateLocked) {
        return;
    }

    m_utcDateLocked = utcDateLocked;
    emit utcDateLockedChanged();
    if (m_utcDateLocked || m_utcTimeLocked) {
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
    }
}

void SkyContextController::setUtcTimeLocked(const bool utcTimeLocked)
{
    m_restoreUtcLockStateOnLiveResume = false;

    if (m_utcTimeLocked == utcTimeLocked) {
        return;
    }

    m_utcTimeLocked = utcTimeLocked;
    emit utcTimeLockedChanged();
    if (m_utcDateLocked || m_utcTimeLocked) {
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
    }
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

    const double clamped = std::clamp(magnitudeCutoff, kMagnitudeCutoffMin, kMagnitudeCutoffMax);
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

    const double nextAltitudeDeg = clampAltitudeDeg(altitudeDeg);
    const double nextAzimuthDeg = normalizeAzimuthDeg(azimuthDeg);
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

    const double wheelSteps = static_cast<double>(wheelDeltaY) / kWheelAngleDeltaStep;
    const double zoomMultiplier = std::pow(kWheelZoomStepScale, wheelSteps);
    setViewFieldOfViewDeg(m_viewFieldOfViewDeg * zoomMultiplier);
}

void SkyContextController::resetViewDirection()
{
    setViewCenter(kDefaultViewportCenterAltitudeDeg, kDefaultViewportCenterAzimuthDeg);
}

void SkyContextController::stepForward()
{
    if (m_utcDateLocked || m_utcTimeLocked) {
        m_restoreUtcLockStateOnLiveResume = true;
        m_restoreUtcDateLockedOnLiveResume = m_utcDateLocked;
        m_restoreUtcTimeLockedOnLiveResume = m_utcTimeLocked;
    }

    if (m_utcDateLocked) {
        m_utcDateLocked = false;
        emit utcDateLockedChanged();
    }
    if (m_utcTimeLocked) {
        m_utcTimeLocked = false;
        emit utcTimeLockedChanged();
    }
    setLive(false);
    stepBySeconds(m_stepSeconds);
}

void SkyContextController::stepBackward()
{
    if (m_utcDateLocked || m_utcTimeLocked) {
        m_restoreUtcLockStateOnLiveResume = true;
        m_restoreUtcDateLockedOnLiveResume = m_utcDateLocked;
        m_restoreUtcTimeLockedOnLiveResume = m_utcTimeLocked;
    }

    if (m_utcDateLocked) {
        m_utcDateLocked = false;
        emit utcDateLockedChanged();
    }
    if (m_utcTimeLocked) {
        m_utcTimeLocked = false;
        emit utcTimeLockedChanged();
    }
    setLive(false);
    stepBySeconds(-m_stepSeconds);
}

void SkyContextController::setUtcDateText(const QString& utcDateText)
{
    if (m_utcDateLocked) {
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
        return;
    }

    const auto date = QDate::fromString(utcDateText, "yyyy-MM-dd");
    if (!date.isValid()) {
        emit invalidUtcDateInput(utcDateText);
        emit utcDateTextChanged();
        return;
    }

    const auto currentUtc = toQDateTimeUtc(m_skyContext.utcTime);
    const QDateTime nextUtc(
        date,
        currentUtc.time(),
        QTimeZone::UTC
    );

    setCurrentUtc(nextUtc);
    setLive(false);
}

void SkyContextController::setUtcTimeText(const QString& utcTimeText)
{
    if (m_utcTimeLocked) {
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
        return;
    }

    const auto time = QTime::fromString(utcTimeText, "HH:mm:ss");
    if (!time.isValid()) {
        emit invalidUtcTimeInput(utcTimeText);
        emit utcTimeTextChanged();
        return;
    }

    const auto currentUtc = toQDateTimeUtc(m_skyContext.utcTime);
    const QDateTime nextUtc(
        currentUtc.date(),
        time,
        QTimeZone::UTC
    );

    setCurrentUtc(nextUtc);
    setLive(false);
}

void SkyContextController::setLatitudeText(const QString& latitudeText)
{
    bool isValidNumber = false;
    const double latitude = latitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.latitudeDeg = latitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit invalidLatitudeInput(latitudeText);
        emit latitudeTextChanged();
        return;
    }

    if (m_skyContext.observer.latitudeDeg == nextObserver.latitudeDeg) {
        return;
    }

    m_skyContext.observer = nextObserver;
    emit latitudeTextChanged();
    emit skyContextChanged();
}

void SkyContextController::setLongitudeText(const QString& longitudeText)
{
    bool isValidNumber = false;
    const double longitude = longitudeText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.longitudeDeg = longitude;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit invalidLongitudeInput(longitudeText);
        emit longitudeTextChanged();
        return;
    }

    if (m_skyContext.observer.longitudeDeg == nextObserver.longitudeDeg) {
        return;
    }

    m_skyContext.observer = nextObserver;
    emit longitudeTextChanged();
    emit skyContextChanged();
}

void SkyContextController::setElevationText(const QString& elevationText)
{
    bool isValidNumber = false;
    const double elevation = elevationText.trimmed().toDouble(&isValidNumber);
    skygate::core::GeoLocation nextObserver = m_skyContext.observer;
    nextObserver.elevationMeters = elevation;
    if (!isValidNumber || !nextObserver.isValid()) {
        emit invalidElevationInput(elevationText);
        emit elevationTextChanged();
        return;
    }

    if (m_skyContext.observer.elevationMeters == nextObserver.elevationMeters) {
        return;
    }

    m_skyContext.observer = nextObserver;
    emit elevationTextChanged();
    emit skyContextChanged();
}

void SkyContextController::setProjectionTypeText(const QString& projectionTypeText)
{
    const auto parsedType = projectionTypeFromString(projectionTypeText);
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

    const bool hasUtcLock = m_utcDateLocked || m_utcTimeLocked;
    if (hasUtcLock) {
        // In lock mode, live updates follow current UTC rather than timeline speed.
        setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime));
        return;
    }

    m_speedRemainderSeconds += m_speedMultiplier;
    const int wholeSeconds = static_cast<int>(std::floor(m_speedRemainderSeconds));
    if (wholeSeconds <= 0) {
        return;
    }

    m_speedRemainderSeconds -= wholeSeconds;
    stepBySeconds(wholeSeconds);
}

void SkyContextController::stepBySeconds(const int stepSeconds)
{
    if (stepSeconds == 0) {
        return;
    }

    if (!m_live) {
        m_speedRemainderSeconds = 0.0;
    }

    setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime).addSecs(stepSeconds));
}

void SkyContextController::setCurrentUtc(const QDateTime& utcTime)
{
    QDateTime normalizedUtc = utcTime.toUTC();
    if (m_utcDateLocked || m_utcTimeLocked) {
        const QDateTime currentUtc = currentUtcDateTime();
        if (m_utcDateLocked) {
            normalizedUtc.setDate(currentUtc.date());
        }
        if (m_utcTimeLocked) {
            normalizedUtc.setTime(currentUtc.time());
        }
    }

    const auto nextUtc = toUtcTimePoint(normalizedUtc);
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

    const double nextViewFieldOfViewDeg = clampFieldOfViewDeg(viewFieldOfViewDeg);
    if (std::abs(m_viewFieldOfViewDeg - nextViewFieldOfViewDeg) < 1e-9) {
        return;
    }

    m_viewFieldOfViewDeg = nextViewFieldOfViewDeg;
    emit viewDirectionChanged();
    emit skyContextChanged();
}

void SkyContextController::initializeCurrentLocation()
{
#if SKYGATE_HAS_POSITIONING
    m_locationStatusText = "Location: Initializing";
    emit locationStatusTextChanged();

    m_positionSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_positionSource == nullptr) {
        m_locationStatusText = "Location: Device source unavailable";
        emit locationStatusTextChanged();
        return;
    }

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
            m_locationStatusText = "Location: Update failed";
            emit locationStatusTextChanged();
        }
    );

    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Precise);
    permission.setAvailability(QLocationPermission::WhenInUse);

    auto startLocationUpdate = [this] {
        m_positionSource->requestUpdate(kLocationUpdateTimeoutMs);
    };

    QCoreApplication* app = QCoreApplication::instance();
    if (app == nullptr) {
        return;
    }

    const Qt::PermissionStatus permissionStatus = app->checkPermission(permission);
    if (permissionStatus == Qt::PermissionStatus::Granted) {
        m_locationStatusText = "Location: Locating";
        emit locationStatusTextChanged();
        startLocationUpdate();
        return;
    }

    if (permissionStatus == Qt::PermissionStatus::Undetermined) {
        m_locationStatusText = "Location: Waiting for permission";
        emit locationStatusTextChanged();
        app->requestPermission(
            permission,
            this,
            [this, startLocationUpdate](const QPermission& requestPermission) {
                if (requestPermission.status() == Qt::PermissionStatus::Granted) {
                    m_locationStatusText = "Location: Locating";
                    emit locationStatusTextChanged();
                    startLocationUpdate();
                    return;
                }

                m_locationStatusText = "Location: Permission denied";
                emit locationStatusTextChanged();
            }
        );
        return;
    }

    m_locationStatusText = "Location: Permission denied";
    emit locationStatusTextChanged();
#else
    m_locationStatusText = "Location: Positioning unavailable";
    emit locationStatusTextChanged();
#endif
}

void SkyContextController::applyCurrentLocation(const QGeoPositionInfo& positionInfo)
{
#if SKYGATE_HAS_POSITIONING
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

    const bool latitudeChanged = m_skyContext.observer.latitudeDeg != nextObserver.latitudeDeg;
    const bool longitudeChanged = m_skyContext.observer.longitudeDeg != nextObserver.longitudeDeg;
    const bool elevationChanged = m_skyContext.observer.elevationMeters != nextObserver.elevationMeters;

    if (!latitudeChanged && !longitudeChanged && !elevationChanged) {
        return;
    }

    m_skyContext.observer = nextObserver;
    m_locationStatusText = "Location: Using current location";
    emit locationStatusTextChanged();

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
