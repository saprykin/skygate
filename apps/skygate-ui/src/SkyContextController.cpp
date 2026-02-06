#include "SkyContextController.hpp"

#include <Qt>
#include <QCoreApplication>
#include <QSettings>
#include <QTimeZone>

#include "skygate/core/ProjectionFactory.hpp"

#include <cmath>
#include <chrono>
#include <optional>
#include <utility>

#if SKYGATE_HAS_POSITIONING
#include <QGeoCoordinate>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QLocationPermission>
#endif

namespace {
constexpr auto kTickIntervalMs = 1000;
constexpr auto kLocationUpdateTimeoutMs = 5000;
constexpr auto kSettingsVersion = 1;

QString formatCoordinate(double value)
{
    return QString::number(value, 'f', 6);
}

QString formatElevation(double value)
{
    return QString::number(value, 'f', 1);
}

QString projectionTypeToString(const skygate::core::ProjectionType projectionType)
{
    switch (projectionType) {
    case skygate::core::ProjectionType::Stereographic:
        return "Stereographic";
    case skygate::core::ProjectionType::AzimuthalEquidistant:
        return "AzimuthalEquidistant";
    }

    return "Unknown";
}

std::optional<skygate::core::ProjectionType> projectionTypeFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "stereographic") {
        return skygate::core::ProjectionType::Stereographic;
    }

    if (normalized == "azimuthalequidistant") {
        return skygate::core::ProjectionType::AzimuthalEquidistant;
    }

    return std::nullopt;
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

QString settingsKey(const QString& name)
{
    return QString("skyContext/%1").arg(name);
}
}

SkyContextController::SkyContextController(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog,
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine,
    QObject* parent
)
    : QObject(parent)
    , m_starCatalog(std::move(starCatalog))
    , m_ephemerisEngine(std::move(ephemerisEngine))
{
    m_projection = skygate::core::createProjection(m_projectionType);
    m_skyContext.utcTime = toUtcTimePoint(QDateTime::currentDateTimeUtc().toUTC());
    loadSettings();

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

QString SkyContextController::projectionTypeText() const
{
    return projectionTypeToString(m_projectionType);
}

QString SkyContextController::projectionSampleText() const
{
    if (m_projection == nullptr) {
        return "Projection unavailable";
    }

    skygate::core::ProjectionParams params;
    params.center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0};
    params.fovDeg = 90.0;
    params.rollDeg = 0.0;
    params.viewportWidth = 1100.0;
    params.viewportHeight = 760.0;

    const auto projected = m_projection->project(
        skygate::core::HorizontalCoordinate {.altitudeDeg = 30.0, .azimuthDeg = 180.0},
        params
    );

    return QString("Sample x=%1 y=%2 visible=%3").arg(
        QString::number(projected.x, 'f', 1),
        QString::number(projected.y, 'f', 1),
        projected.isVisible ? "true" : "false"
    );
}

QString SkyContextController::locationStatusText() const
{
    return m_locationStatusText;
}

QString SkyContextController::skyContextSummary() const
{
    QString bodyCountText = "n/a";
    if (m_ephemerisEngine != nullptr) {
        const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
        bodyCountText = QString::number(snapshot.states.size());
    }

    return QString(
        "UTC %1 %2 | Lat %3 | Lon %4 | Elev %5 m | Proj %6 | Bodies %7"
    ).arg(
        utcDateText(),
        utcTimeText(),
        latitudeText(),
        longitudeText(),
        elevationText(),
        projectionTypeText(),
        bodyCountText
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
    emit skyContextChanged();
}

bool SkyContextController::saveSettings() const
{
    QSettings settings;
    settings.setValue(settingsKey("version"), kSettingsVersion);
    settings.setValue(settingsKey("live"), m_live);
    settings.setValue(settingsKey("utcEpochSeconds"), toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch());
    settings.setValue(settingsKey("latitudeDeg"), m_skyContext.observer.latitudeDeg);
    settings.setValue(settingsKey("longitudeDeg"), m_skyContext.observer.longitudeDeg);
    settings.setValue(settingsKey("elevationMeters"), m_skyContext.observer.elevationMeters);
    settings.setValue(settingsKey("projectionType"), projectionTypeText());
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool SkyContextController::loadSettings()
{
    QSettings settings;
    if (!settings.contains(settingsKey("version"))) {
        return false;
    }

    const bool live = settings.value(settingsKey("live"), m_live).toBool();
    const qint64 utcEpochSeconds = settings.value(
        settingsKey("utcEpochSeconds"),
        toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch()
    ).toLongLong();
    const double latitudeDeg = settings.value(
        settingsKey("latitudeDeg"),
        m_skyContext.observer.latitudeDeg
    ).toDouble();
    const double longitudeDeg = settings.value(
        settingsKey("longitudeDeg"),
        m_skyContext.observer.longitudeDeg
    ).toDouble();
    const double elevationMeters = settings.value(
        settingsKey("elevationMeters"),
        m_skyContext.observer.elevationMeters
    ).toDouble();
    const QString projectionType = settings.value(
        settingsKey("projectionType"),
        projectionTypeText()
    ).toString();

    setLive(live);
    setCurrentUtc(QDateTime::fromSecsSinceEpoch(utcEpochSeconds, QTimeZone::UTC));
    setLatitudeText(QString::number(latitudeDeg, 'f', 6));
    setLongitudeText(QString::number(longitudeDeg, 'f', 6));
    setElevationText(QString::number(elevationMeters, 'f', 1));
    setProjectionTypeText(projectionType);

    return true;
}
