#include "SkyContextController.hpp"

#include <Qt>
#include <QCoreApplication>
#include <QTimeZone>

#include <cmath>
#include <chrono>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QLocationPermission>
#endif

namespace {
constexpr auto kTickIntervalMs = 1000;
constexpr auto kLocationUpdateTimeoutMs = 5000;

QString formatCoordinate(double value)
{
    return QString::number(value, 'f', 6);
}

QString formatElevation(double value)
{
    return QString::number(value, 'f', 1);
}

QDateTime toQDateTimeUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(utcTime.time_since_epoch());
    return QDateTime::fromSecsSinceEpoch(seconds.count(), QTimeZone::UTC);
}

skygate::core::UtcTimePoint toUtcTimePoint(const QDateTime& utcTime)
{
    return skygate::core::UtcTimePoint(std::chrono::seconds(utcTime.toSecsSinceEpoch()));
}
}

SkyContextController::SkyContextController(QObject* parent)
    : QObject(parent)
{
    m_skyContext.utcTime = toUtcTimePoint(QDateTime::currentDateTimeUtc().toUTC());

    m_timer.setInterval(kTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();

    initializeCurrentLocation();
}

bool SkyContextController::live() const noexcept
{
    return m_live;
}

QString SkyContextController::utcTimeText() const
{
    return toQDateTimeUtc(m_skyContext.utcTime).toString("HH:mm:ss");
}

QString SkyContextController::utcDateText() const
{
    return toQDateTimeUtc(m_skyContext.utcTime).toString("yyyy-MM-dd");
}

QString SkyContextController::latitudeText() const
{
    return formatCoordinate(m_skyContext.observer.latitudeDeg);
}

QString SkyContextController::longitudeText() const
{
    return formatCoordinate(m_skyContext.observer.longitudeDeg);
}

QString SkyContextController::elevationText() const
{
    return formatElevation(m_skyContext.observer.elevationMeters);
}

QString SkyContextController::locationStatusText() const
{
    return m_locationStatusText;
}

QString SkyContextController::skyContextSummary() const
{
    return QString(
        "UTC %1 %2 | Lat %3 | Lon %4 | Elev %5 m"
    ).arg(
        utcDateText(),
        utcTimeText(),
        latitudeText(),
        longitudeText(),
        elevationText()
    );
}

const skygate::core::SkyContext& SkyContextController::skyContext() const noexcept
{
    return m_skyContext;
}

void SkyContextController::setLive(bool live)
{
    if (m_live == live) {
        return;
    }

    m_live = live;
    emit liveChanged();
}

void SkyContextController::setUtcDateText(const QString& utcDateText)
{
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

void SkyContextController::tickUtcTime()
{
    if (!m_live) {
        return;
    }

    setCurrentUtc(toQDateTimeUtc(m_skyContext.utcTime).addSecs(1));
}

void SkyContextController::setCurrentUtc(const QDateTime& utcTime)
{
    const auto nextUtc = toUtcTimePoint(utcTime);
    if (m_skyContext.utcTime == nextUtc) {
        return;
    }

    m_skyContext.utcTime = nextUtc;
    emit utcDateTextChanged();
    emit utcTimeTextChanged();
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
