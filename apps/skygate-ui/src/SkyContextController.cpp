#include "SkyContextController.hpp"
#include "CatalogCoordinator.hpp"

#include <Qt>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>
#include <QColor>
#include <QNetworkAccessManager>

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <cmath>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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
constexpr double kDefaultViewportCenterAltitudeDeg = 45.0;
constexpr double kDefaultViewportCenterAzimuthDeg = 180.0;
constexpr double kViewportFieldOfViewMinDeg = 20.0;
constexpr double kViewportFieldOfViewMaxDeg = 150.0;
constexpr double kWheelZoomStepScale = 0.90;
constexpr double kWheelAngleDeltaStep = 120.0;
constexpr double kMagnitudeCutoffMin = -2.0;
constexpr double kMagnitudeCutoffMax = 12.0;
constexpr double kViewAltitudeMinDeg = -90.0;
constexpr double kViewAltitudeMaxDeg = 90.0;
constexpr const char* kCatalogCacheFileName = "catalog-cache-v1.txt";
constexpr const char* kHygCatalogPrimaryUrl =
    "https://astronexus.com/downloads/catalogs/hygdata_v42.csv.gz";
constexpr const char* kHygCatalogMirrorUrl =
    "https://astronexus.com/downloads/catalogs/hygdata_v41.csv.gz";
constexpr const char* kHygCatalogGithubMirrorUrl =
    "https://raw.githubusercontent.com/astronexus/HYG-Database/master/hygdata_v3.csv";
constexpr std::string_view kStarterCatalogRows =
    "sun|Sun|Sun|-26.74\n"
    "moon|Moon|Moon|-12.74\n"
    "venus|Venus|Planet|-4.92\n"
    "jupiter|Jupiter|Planet|-2.94\n"
    "sirius|Sirius|Star|-1.46\n"
    "vega|Vega|Star|0.03\n"
    "betelgeuse|Betelgeuse|Star|0.50\n";
constexpr std::string_view kMajorConstellationsCatalogRows =
    "sirius|Sirius|Star|-1.46\n"
    "canopus|Canopus|Star|-0.74\n"
    "arcturus|Arcturus|Star|-0.05\n"
    "vega|Vega|Star|0.03\n"
    "capella|Capella|Star|0.08\n"
    "rigel|Rigel|Star|0.12\n"
    "procyon|Procyon|Star|0.34\n"
    "betelgeuse|Betelgeuse|Star|0.50\n"
    "orion|Orion|Constellation|1.6\n"
    "ursa_major|Ursa Major|Constellation|1.8\n"
    "ursa_minor|Ursa Minor|Constellation|2.1\n"
    "cassiopeia|Cassiopeia|Constellation|2.2\n"
    "scorpius|Scorpius|Constellation|1.7\n"
    "cygnus|Cygnus|Constellation|1.3\n"
    "taurus|Taurus|Constellation|1.7\n"
    "leo|Leo|Constellation|1.4\n"
    "gemini|Gemini|Constellation|1.6\n"
    "andromeda|Andromeda|Constellation|2.1\n";

struct ConstellationLineRef {
    std::string_view startId;
    std::string_view endId;
};

constexpr std::array<ConstellationLineRef, 57> kConstellationLineRefs = {{
    // Bundled fallback.
    {.startId = "sirius", .endId = "procyon"},
    {.startId = "procyon", .endId = "betelgeuse"},
    {.startId = "betelgeuse", .endId = "rigel"},
    {.startId = "rigel", .endId = "sirius"},
    {.startId = "betelgeuse", .endId = "capella"},
    {.startId = "capella", .endId = "vega"},
    {.startId = "vega", .endId = "arcturus"},
    // Orion.
    {.startId = "hip_27989", .endId = "hip_25336"},
    {.startId = "hip_27989", .endId = "hip_26727"},
    {.startId = "hip_25336", .endId = "hip_25930"},
    {.startId = "hip_25930", .endId = "hip_26311"},
    {.startId = "hip_26311", .endId = "hip_26727"},
    {.startId = "hip_26727", .endId = "hip_27366"},
    {.startId = "hip_27366", .endId = "hip_24436"},
    {.startId = "hip_24436", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_25930"},
    {.startId = "hip_26207", .endId = "hip_27989"},
    // Ursa Major.
    {.startId = "hip_67301", .endId = "hip_65378"},
    {.startId = "hip_65378", .endId = "hip_62956"},
    {.startId = "hip_62956", .endId = "hip_59774"},
    {.startId = "hip_59774", .endId = "hip_54061"},
    {.startId = "hip_54061", .endId = "hip_53910"},
    {.startId = "hip_53910", .endId = "hip_58001"},
    // Ursa Minor.
    {.startId = "hip_11767", .endId = "hip_79822"},
    {.startId = "hip_79822", .endId = "hip_77055"},
    {.startId = "hip_77055", .endId = "hip_75097"},
    {.startId = "hip_75097", .endId = "hip_72607"},
    {.startId = "hip_72607", .endId = "hip_85822"},
    {.startId = "hip_85822", .endId = "hip_82080"},
    {.startId = "hip_82080", .endId = "hip_11767"},
    // Cassiopeia.
    {.startId = "hip_746", .endId = "hip_3179"},
    {.startId = "hip_3179", .endId = "hip_4427"},
    {.startId = "hip_4427", .endId = "hip_6686"},
    {.startId = "hip_6686", .endId = "hip_8886"},
    // Cygnus.
    {.startId = "hip_102098", .endId = "hip_100453"},
    {.startId = "hip_100453", .endId = "hip_95947"},
    {.startId = "hip_100453", .endId = "hip_98110"},
    {.startId = "hip_100453", .endId = "hip_97165"},
    {.startId = "hip_98110", .endId = "hip_95947"},
    // Taurus.
    {.startId = "hip_20889", .endId = "hip_21421"},
    {.startId = "hip_20889", .endId = "hip_25428"},
    {.startId = "hip_21421", .endId = "hip_25428"},
    // Gemini.
    {.startId = "hip_31681", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_35550"},
    {.startId = "hip_37826", .endId = "hip_34088"},
    {.startId = "hip_34088", .endId = "hip_32362"},
    // Leo.
    {.startId = "hip_49669", .endId = "hip_54872"},
    {.startId = "hip_54872", .endId = "hip_57632"},
    {.startId = "hip_54872", .endId = "hip_50583"},
    // Andromeda.
    {.startId = "hip_677", .endId = "hip_3092"},
    {.startId = "hip_3092", .endId = "hip_5447"},
    {.startId = "hip_5447", .endId = "hip_9640"},
    // Scorpius.
    {.startId = "hip_78265", .endId = "hip_78401"},
    {.startId = "hip_78401", .endId = "hip_78820"},
    {.startId = "hip_78820", .endId = "hip_80763"},
    {.startId = "hip_80763", .endId = "hip_85927"},
    {.startId = "hip_85927", .endId = "hip_86228"},
}};

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

QString bodyTypeToString(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Star:
        return "Star";
    case skygate::ephemeris::CelestialBodyType::Planet:
        return "Planet";
    case skygate::ephemeris::CelestialBodyType::Moon:
        return "Moon";
    case skygate::ephemeris::CelestialBodyType::Sun:
        return "Sun";
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return "Constellation";
    }

    return "Star";
}

QString defaultCatalogCachePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(QString::fromUtf8(kCatalogCacheFileName));
}

QString sanitizeCatalogField(QString value)
{
    value.replace('|', '/');
    value.replace('\n', ' ');
    value.replace('\r', ' ');
    return value.trimmed();
}

QByteArray serializeCatalogRows(const std::vector<skygate::ephemeris::CelestialBody>& bodies)
{
    QByteArray rows;
    rows.reserve(static_cast<int>(bodies.size() * 64));
    for (const auto& body : bodies) {
        const QString id = sanitizeCatalogField(QString::fromStdString(body.id));
        const QString displayName = sanitizeCatalogField(QString::fromStdString(body.displayName));
        if (id.isEmpty() || displayName.isEmpty()) {
            continue;
        }

        rows.append(id.toUtf8());
        rows.append('|');
        rows.append(displayName.toUtf8());
        rows.append('|');
        rows.append(bodyTypeToString(body.type).toUtf8());
        rows.append('|');
        rows.append(QByteArray::number(body.visualMagnitude, 'g', 17));
        if (body.fixedEquatorial.has_value()) {
            rows.append('|');
            rows.append(QByteArray::number(body.fixedEquatorial->rightAscensionHours, 'g', 17));
            rows.append('|');
            rows.append(QByteArray::number(body.fixedEquatorial->declinationDeg, 'g', 17));
        }
        rows.append('\n');
    }
    return rows;
}

double pointSizeForMagnitude(const double magnitude)
{
    const double normalizedBrightness = std::clamp(1.0 - ((magnitude + 1.5) / 8.0), 0.2, 1.0);
    return 1.8 + normalizedBrightness * 5.0;
}

QColor colorForBodyType(const skygate::ephemeris::CelestialBodyType type)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return QColor(255, 214, 128, 230);
    case skygate::ephemeris::CelestialBodyType::Moon:
        return QColor(214, 224, 250, 220);
    case skygate::ephemeris::CelestialBodyType::Planet:
        return QColor(255, 188, 140, 220);
    case skygate::ephemeris::CelestialBodyType::Star:
        return QColor(188, 214, 255, 210);
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return QColor(160, 244, 200, 205);
    }

    return QColor(220, 220, 240, 200);
}

QColor constellationLineColor()
{
    return QColor(146, 205, 255, 132);
}

std::string_view hipSuffix(std::string_view value)
{
    constexpr std::string_view kHipPrefix = "hip_";
    if (!value.starts_with(kHipPrefix)) {
        return {};
    }
    return value.substr(kHipPrefix.size());
}

double normalizeAzimuthDeg(double azimuthDeg)
{
    const double normalized = std::fmod(azimuthDeg, 360.0);
    if (normalized < 0.0) {
        return normalized + 360.0;
    }

    return normalized;
}

double clampAltitudeDeg(const double altitudeDeg)
{
    return std::clamp(altitudeDeg, kViewAltitudeMinDeg, kViewAltitudeMaxDeg);
}

double clampFieldOfViewDeg(const double fieldOfViewDeg)
{
    return std::clamp(fieldOfViewDeg, kViewportFieldOfViewMinDeg, kViewportFieldOfViewMaxDeg);
}

skygate::core::ProjectionParams buildProjectionParams(
    const double viewportWidth,
    const double viewportHeight,
    const double centerAltitudeDeg,
    const double centerAzimuthDeg,
    const double fieldOfViewDeg
)
{
    skygate::core::ProjectionParams projectionParams;
    projectionParams.center = {
        .altitudeDeg = centerAltitudeDeg,
        .azimuthDeg = centerAzimuthDeg
    };
    projectionParams.fovDeg = fieldOfViewDeg;
    projectionParams.rollDeg = 0.0;
    projectionParams.viewportWidth = viewportWidth;
    projectionParams.viewportHeight = viewportHeight;
    return projectionParams;
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
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_catalogCoordinator = std::make_unique<CatalogCoordinator>(m_networkAccessManager);

    if (m_starCatalog == nullptr) {
        m_starCatalog = skygate::ephemeris::createBundledStarCatalog();
    }
    if (m_ephemerisEngine == nullptr && m_starCatalog != nullptr) {
        m_ephemerisEngine = skygate::ephemeris::createEphemerisEngineStub(m_starCatalog.get());
    }

    if (m_starCatalog != nullptr) {
        applyCatalog(std::move(m_starCatalog), "Bundled", false);
    } else {
        m_catalogStatusText = "Catalog: Unavailable";
    }

    m_projection = skygate::core::createProjection(m_projectionType);
    m_skyContext.utcTime = toUtcTimePoint(QDateTime::currentDateTimeUtc().toUTC());
    loadSettings();

    m_timer.setInterval(kTickIntervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SkyContextController::tickUtcTime);
    m_timer.start();

    initializeCurrentLocation();
}

SkyContextController::~SkyContextController() = default;

bool SkyContextController::live() const noexcept
{
    return m_live;
}

double SkyContextController::speedMultiplier() const noexcept
{
    return m_speedMultiplier;
}

int SkyContextController::stepSeconds() const noexcept
{
    return m_stepSeconds;
}

double SkyContextController::magnitudeCutoff() const noexcept
{
    return m_magnitudeCutoff;
}

double SkyContextController::viewCenterAltitudeDeg() const noexcept
{
    return m_viewCenterAltitudeDeg;
}

double SkyContextController::viewCenterAzimuthDeg() const noexcept
{
    return m_viewCenterAzimuthDeg;
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
    params.center = {.altitudeDeg = m_viewCenterAltitudeDeg, .azimuthDeg = m_viewCenterAzimuthDeg};
    params.fovDeg = m_viewFieldOfViewDeg;
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

QString SkyContextController::catalogStatusText() const
{
    return m_catalogStatusText;
}

bool SkyContextController::downloadingCatalog() const noexcept
{
    return m_downloadingCatalog;
}

QString SkyContextController::skyContextSummary() const
{
    QString bodyCountText = "n/a";
    if (m_ephemerisEngine != nullptr) {
        const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
        bodyCountText = QString::number(snapshot.states.size());
    }

    return QString(
        "UTC %1 %2 | Lat %3 | Lon %4 | Elev %5 m | Proj %6 | View Alt %7 Az %8 | FOV %9 | Bodies %10"
    ).arg(
        utcDateText(),
        utcTimeText(),
        latitudeText(),
        longitudeText(),
        elevationText(),
        projectionTypeText(),
        QString::number(m_viewCenterAltitudeDeg, 'f', 1),
        QString::number(m_viewCenterAzimuthDeg, 'f', 1),
        QString::number(m_viewFieldOfViewDeg, 'f', 1),
        bodyCountText
    );
}

const skygate::core::SkyContext& SkyContextController::skyContext() const noexcept
{
    return m_skyContext;
}

std::vector<SkyContextController::SkyRenderPoint> SkyContextController::renderPoints(
    const double viewportWidth,
    const double viewportHeight
) const
{
    if (viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }

    if (m_ephemerisEngine == nullptr || m_projection == nullptr) {
        return {};
    }

    const skygate::core::ProjectionParams projectionParams = buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );

    const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
    std::vector<SkyRenderPoint> points;
    points.reserve(snapshot.states.size());

    for (const auto& state : snapshot.states) {
        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
        ) {
            continue;
        }

        if (
            state.body.type == skygate::ephemeris::CelestialBodyType::Star
            && state.body.visualMagnitude > m_magnitudeCutoff
        ) {
            continue;
        }

        const auto projected = m_projection->project(state.horizontal, projectionParams);
        if (!projected.isVisible) {
            continue;
        }

        SkyRenderPoint point;
        point.x = projected.x;
        point.y = projected.y;
        point.sizePx = pointSizeForMagnitude(state.body.visualMagnitude);
        if (state.body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            point.sizePx = std::max(point.sizePx, 3.0);
        }
        point.displayName = QString::fromStdString(state.body.displayName);
        point.color = colorForBodyType(state.body.type);
        points.push_back(point);
    }

    return points;
}

std::vector<SkyContextController::SkyRenderLine> SkyContextController::renderConstellationLines(
    const double viewportWidth,
    const double viewportHeight
) const
{
    if (viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }

    if (m_ephemerisEngine == nullptr || m_projection == nullptr) {
        return {};
    }

    const skygate::core::ProjectionParams projectionParams = buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );

    const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> horizontalById;
    std::unordered_map<std::string_view, skygate::core::HorizontalCoordinate> horizontalByDisplayName;
    horizontalById.reserve(snapshot.states.size());
    horizontalByDisplayName.reserve(snapshot.states.size());

    for (const auto& state : snapshot.states) {
        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
        ) {
            continue;
        }
        horizontalById[state.body.id] = state.horizontal;
        horizontalByDisplayName[state.body.displayName] = state.horizontal;
    }

    const auto findHorizontal = [&](const std::string_view lineRefId) -> const skygate::core::HorizontalCoordinate* {
        if (const auto idIt = horizontalById.find(lineRefId); idIt != horizontalById.end()) {
            return &idIt->second;
        }

        const std::string_view hipNumber = hipSuffix(lineRefId);
        if (!hipNumber.empty()) {
            std::string legacyHipDisplayName = "HIP ";
            legacyHipDisplayName.append(hipNumber.data(), hipNumber.size());
            if (
                const auto displayNameIt = horizontalByDisplayName.find(legacyHipDisplayName);
                displayNameIt != horizontalByDisplayName.end()
            ) {
                return &displayNameIt->second;
            }
        }

        return nullptr;
    };

    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.90;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    std::vector<SkyRenderLine> lines;
    lines.reserve(kConstellationLineRefs.size());

    for (const auto& lineRef : kConstellationLineRefs) {
        const auto* startHorizontal = findHorizontal(lineRef.startId);
        const auto* endHorizontal = findHorizontal(lineRef.endId);
        if (startHorizontal == nullptr || endHorizontal == nullptr) {
            continue;
        }

        const auto startProjected = m_projection->project(*startHorizontal, projectionParams);
        const auto endProjected = m_projection->project(*endHorizontal, projectionParams);
        if (!startProjected.isVisible || !endProjected.isVisible) {
            continue;
        }

        const double deltaX = endProjected.x - startProjected.x;
        const double deltaY = endProjected.y - startProjected.y;
        const double lengthSquared = deltaX * deltaX + deltaY * deltaY;
        if (lengthSquared > maxSegmentLengthSquared) {
            continue;
        }

        SkyRenderLine line;
        line.x1 = startProjected.x;
        line.y1 = startProjected.y;
        line.x2 = endProjected.x;
        line.y2 = endProjected.y;
        line.color = constellationLineColor();
        lines.push_back(line);
    }

    return lines;
}

skygate::core::ScreenPoint SkyContextController::projectHorizontal(
    const skygate::core::HorizontalCoordinate& coordinate,
    const double viewportWidth,
    const double viewportHeight
) const noexcept
{
    if (m_projection == nullptr || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }

    const skygate::core::ProjectionParams projectionParams = buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );
    return m_projection->project(coordinate, projectionParams);
}

void SkyContextController::setLive(bool live)
{
    if (m_live == live) {
        return;
    }

    m_live = live;
    m_speedRemainderSeconds = 0.0;
    emit liveChanged();
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
    setLive(false);
    stepBySeconds(m_stepSeconds);
}

void SkyContextController::stepBackward()
{
    setLive(false);
    stepBySeconds(-m_stepSeconds);
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
    const auto nextUtc = toUtcTimePoint(utcTime);
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
    settings.setValue(settingsKey("speedMultiplier"), m_speedMultiplier);
    settings.setValue(settingsKey("stepSeconds"), m_stepSeconds);
    settings.setValue(settingsKey("magnitudeCutoff"), m_magnitudeCutoff);
    settings.setValue(settingsKey("viewCenterAltitudeDeg"), m_viewCenterAltitudeDeg);
    settings.setValue(settingsKey("viewCenterAzimuthDeg"), m_viewCenterAzimuthDeg);
    settings.setValue(settingsKey("viewFieldOfViewDeg"), m_viewFieldOfViewDeg);
    settings.setValue(settingsKey("utcEpochSeconds"), toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch());
    settings.setValue(settingsKey("latitudeDeg"), m_skyContext.observer.latitudeDeg);
    settings.setValue(settingsKey("longitudeDeg"), m_skyContext.observer.longitudeDeg);
    settings.setValue(settingsKey("elevationMeters"), m_skyContext.observer.elevationMeters);
    settings.setValue(settingsKey("projectionType"), projectionTypeText());
    settings.setValue(settingsKey("catalogSourceLabel"), m_catalogSourceLabel);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool SkyContextController::loadSettings()
{
    QSettings settings;
    const bool hasSavedContext = settings.contains(settingsKey("version"));
    if (hasSavedContext) {
        const bool live = settings.value(settingsKey("live"), m_live).toBool();
        const double speedMultiplier = settings.value(
            settingsKey("speedMultiplier"),
            m_speedMultiplier
        ).toDouble();
        const int stepSeconds = settings.value(
            settingsKey("stepSeconds"),
            m_stepSeconds
        ).toInt();
        const double magnitudeCutoff = settings.value(
            settingsKey("magnitudeCutoff"),
            m_magnitudeCutoff
        ).toDouble();
        const double viewCenterAltitudeDeg = settings.value(
            settingsKey("viewCenterAltitudeDeg"),
            m_viewCenterAltitudeDeg
        ).toDouble();
        const double viewCenterAzimuthDeg = settings.value(
            settingsKey("viewCenterAzimuthDeg"),
            m_viewCenterAzimuthDeg
        ).toDouble();
        const double viewFieldOfViewDeg = settings.value(
            settingsKey("viewFieldOfViewDeg"),
            m_viewFieldOfViewDeg
        ).toDouble();
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
        setSpeedMultiplier(speedMultiplier);
        setStepSeconds(stepSeconds);
        setMagnitudeCutoff(magnitudeCutoff);
        setViewCenter(viewCenterAltitudeDeg, viewCenterAzimuthDeg);
        setViewFieldOfViewDeg(viewFieldOfViewDeg);
        setCurrentUtc(QDateTime::fromSecsSinceEpoch(utcEpochSeconds, QTimeZone::UTC));
        setLatitudeText(QString::number(latitudeDeg, 'f', 6));
        setLongitudeText(QString::number(longitudeDeg, 'f', 6));
        setElevationText(QString::number(elevationMeters, 'f', 1));
        setProjectionTypeText(projectionType);
    }

    restoreCatalogCache();
    return hasSavedContext;
}

void SkyContextController::persistCatalogCache(
    const std::vector<skygate::ephemeris::CelestialBody>& bodies,
    const QString& sourceLabel
) const
{
    if (bodies.empty()) {
        return;
    }

    QByteArray rows = serializeCatalogRows(bodies);
    if (rows.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        settingsKey("catalogCachePath"),
        defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return;
    }

    const QFileInfo targetInfo(configuredPath);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        return;
    }

    QSaveFile cacheFile(configuredPath);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const qint64 writtenBytes = cacheFile.write(rows);
    if (writtenBytes != rows.size()) {
        cacheFile.cancelWriting();
        return;
    }

    if (!cacheFile.commit()) {
        return;
    }

    settings.setValue(settingsKey("version"), kSettingsVersion);
    settings.setValue(settingsKey("catalogCachePath"), configuredPath);
    settings.setValue(settingsKey("catalogSourceLabel"), sourceLabel);
    settings.sync();
}

void SkyContextController::restoreCatalogCache()
{
    QSettings settings;
    const QString configuredPath = settings.value(
        settingsKey("catalogCachePath"),
        defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return;
    }

    QFile cacheFile(configuredPath);
    if (!cacheFile.exists() || !cacheFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray rows = cacheFile.readAll();
    if (rows.isEmpty()) {
        return;
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> restoredCatalog =
        skygate::ephemeris::createStarCatalogFromRows(
            std::string_view(rows.constData(), static_cast<std::size_t>(rows.size()))
        );
    if (restoredCatalog == nullptr) {
        m_catalogStatusText = "Catalog: Saved cache unreadable, using bundled";
        emit catalogStatusTextChanged();
        return;
    }

    const QString sourceLabel = settings.value(
        settingsKey("catalogSourceLabel"),
        QString("Saved")
    ).toString();
    applyCatalog(std::move(restoredCatalog), QString("%1 (saved)").arg(sourceLabel), false);
}

double SkyContextController::projectedX(
    const double altitudeDeg,
    const double azimuthDeg,
    const double viewportWidth,
    const double viewportHeight
) const
{
    return projectHorizontal(
        skygate::core::HorizontalCoordinate {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg},
        viewportWidth,
        viewportHeight
    ).x;
}

double SkyContextController::projectedY(
    const double altitudeDeg,
    const double azimuthDeg,
    const double viewportWidth,
    const double viewportHeight
) const
{
    return projectHorizontal(
        skygate::core::HorizontalCoordinate {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg},
        viewportWidth,
        viewportHeight
    ).y;
}

bool SkyContextController::isProjectedVisible(
    const double altitudeDeg,
    const double azimuthDeg,
    const double viewportWidth,
    const double viewportHeight
) const
{
    return projectHorizontal(
        skygate::core::HorizontalCoordinate {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg},
        viewportWidth,
        viewportHeight
    ).isVisible;
}

QString SkyContextController::objectLabelAt(
    const double x,
    const double y,
    const double viewportWidth,
    const double viewportHeight
) const
{
    const auto points = renderPoints(viewportWidth, viewportHeight);
    if (points.empty()) {
        return {};
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    QString bestLabel;

    for (const auto& point : points) {
        if (point.displayName.isEmpty()) {
            continue;
        }

        const double deltaX = x - point.x;
        const double deltaY = y - point.y;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        const double hitRadius = std::max(10.0, point.sizePx + 5.0);
        if (distanceSquared > hitRadius * hitRadius) {
            continue;
        }

        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestLabel = point.displayName;
        }
    }

    return bestLabel;
}

void SkyContextController::loadCatalogPreset(const QString& presetId)
{
    if (m_downloadingCatalog) {
        return;
    }

    const QString normalizedPresetId = presetId.trimmed().toLower();
    if (normalizedPresetId == "bundled") {
        applyCatalog(skygate::ephemeris::createBundledStarCatalog(), "Bundled");
        return;
    }

    if (normalizedPresetId == "starter") {
        applyCatalog(
            skygate::ephemeris::createStarCatalogFromRows(kStarterCatalogRows),
            "Starter"
        );
        return;
    }

    if (normalizedPresetId == "constellations_major") {
        applyCatalog(
            skygate::ephemeris::createStarCatalogFromRows(kMajorConstellationsCatalogRows),
            "Major Constellations"
        );
        return;
    }

    if (normalizedPresetId == "hyg_v3") {
        downloadCatalogFromUrls(
            QStringList {
                QString::fromUtf8(kHygCatalogPrimaryUrl),
                QString::fromUtf8(kHygCatalogMirrorUrl),
                QString::fromUtf8(kHygCatalogGithubMirrorUrl)
            },
            "HYG v3"
        );
        return;
    }

    m_catalogStatusText = QString("Catalog: Unknown preset '%1'").arg(presetId);
    emit catalogStatusTextChanged();
}

void SkyContextController::downloadCatalogFromUrl(const QString& urlText)
{
    downloadCatalogFromUrls(QStringList {urlText}, "Downloaded");
}

void SkyContextController::downloadCatalogFromUrls(const QStringList& urlTexts, const QString& sourceLabel)
{
    if (m_downloadingCatalog) {
        return;
    }

    if (m_catalogCoordinator == nullptr) {
        m_catalogStatusText = "Catalog: Network unavailable";
        emit catalogStatusTextChanged();
        return;
    }

    m_downloadingCatalog = true;
    emit downloadingCatalogChanged();

    m_catalogCoordinator->downloadCatalogFromUrls(
        urlTexts,
        this,
        [this](const QString& statusText) {
            m_catalogStatusText = statusText;
            emit catalogStatusTextChanged();
        },
        [this, sourceLabel](CatalogCoordinator::DownloadResult result) {
            m_downloadingCatalog = false;
            emit downloadingCatalogChanged();

            if (result.catalog == nullptr) {
                m_catalogStatusText = result.errorText;
                emit catalogStatusTextChanged();
                return;
            }

            applyCatalog(std::move(result.catalog), sourceLabel);
        }
    );
}

void SkyContextController::applyCatalog(
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
    const QString& sourceLabel,
    const bool persistCatalog
)
{
    catalog = CatalogCoordinator::ensureCoreSolarSystemBodies(std::move(catalog));
    if (catalog == nullptr) {
        m_catalogStatusText = "Catalog: Failed to load";
        emit catalogStatusTextChanged();
        return;
    }

    const auto bodies = catalog->bodies();
    std::size_t constellationCount = 0;
    for (const auto& body : bodies) {
        if (body.type == skygate::ephemeris::CelestialBodyType::Constellation) {
            ++constellationCount;
        }
    }

    m_starCatalog = std::move(catalog);
    m_ephemerisEngine = skygate::ephemeris::createEphemerisEngineStub(m_starCatalog.get());
    m_catalogSourceLabel = sourceLabel;
    m_catalogStatusText = QString("Catalog: %1 (%2 objects, %3 constellations)").arg(
        sourceLabel,
        QString::number(static_cast<qulonglong>(bodies.size())),
        QString::number(static_cast<qulonglong>(constellationCount))
    );
    if (persistCatalog) {
        persistCatalogCache(bodies, sourceLabel);
    }
    emit catalogStatusTextChanged();
    emit skyContextChanged();
}
