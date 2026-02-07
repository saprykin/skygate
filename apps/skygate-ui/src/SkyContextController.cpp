#include "SkyContextController.hpp"

#include "CatalogCoordinator.hpp"
#include "SkyContextControllerSupport.hpp"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QVariantMap>

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace skygate::ui::internal;

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
    resetConstellationLineRefs();

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
    m_catalogUrlText = QString::fromUtf8(kHygCatalogPrimaryUrl);
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

QString SkyContextController::catalogDatasetInfoText() const
{
    const QLocale locale = QLocale::system();
    return QString("Objects: %1 | Constellations: %2").arg(
        locale.toString(static_cast<qulonglong>(m_catalogBodyCount)),
        locale.toString(static_cast<qulonglong>(m_catalogConstellationCount))
    );
}

int SkyContextController::catalogPresetIndex() const noexcept
{
    return m_catalogPresetIndex;
}

void SkyContextController::setCatalogPresetIndex(int catalogPresetIndex)
{
    if (catalogPresetIndex <= 0) {
        m_catalogPresetIndex = 0;
        return;
    }
    if (catalogPresetIndex == 1) {
        m_catalogPresetIndex = 1;
        return;
    }
    if (catalogPresetIndex == 2) {
        m_catalogPresetIndex = 2;
        return;
    }

    // Backward compatibility with persisted indices from the old 5-option list.
    if (catalogPresetIndex == 3) {
        m_catalogPresetIndex = 1;
        return;
    }
    m_catalogPresetIndex = 2;
}

QString SkyContextController::catalogUrlText() const
{
    return m_catalogUrlText;
}

void SkyContextController::setCatalogUrlText(const QString& catalogUrlText)
{
    const QString normalizedCatalogUrlText = catalogUrlText.trimmed();
    m_catalogUrlText = normalizedCatalogUrlText.isEmpty()
        ? QString::fromUtf8(kHygCatalogPrimaryUrl)
        : normalizedCatalogUrlText;
}

bool SkyContextController::downloadingCatalog() const noexcept
{
    return m_downloadingCatalog;
}

bool SkyContextController::catalogProcessing() const noexcept
{
    return m_catalogProcessing;
}

QString SkyContextController::skyContextSummary() const
{
    QString bodyCountText = "n/a";
    if (m_catalogBodyCount > 0) {
        bodyCountText = QString::number(static_cast<qulonglong>(m_catalogBodyCount));
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
    lines.reserve(m_constellationLineRefs.size());

    for (const auto& lineRef : m_constellationLineRefs) {
        const auto* startHorizontal = findHorizontal(lineRef.first);
        const auto* endHorizontal = findHorizontal(lineRef.second);
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

QVariantList SkyContextController::constellationLabels(
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

    std::unordered_set<std::string> seenLabels;
    QVariantList labels;
    labels.reserve(static_cast<int>((snapshot.states.size() / 8U) + m_constellationLabelRefs.size()));

    constexpr double kEdgeMarginPx = 10.0;
    for (const auto& state : snapshot.states) {
        if (state.body.type != skygate::ephemeris::CelestialBodyType::Constellation) {
            continue;
        }

        if (
            !std::isfinite(state.horizontal.altitudeDeg)
            || !std::isfinite(state.horizontal.azimuthDeg)
            || state.body.displayName.empty()
        ) {
            continue;
        }

        if (!seenLabels.insert(state.body.displayName).second) {
            continue;
        }

        const auto projected = m_projection->project(state.horizontal, projectionParams);
        if (!projected.isVisible) {
            continue;
        }

        if (
            projected.x < kEdgeMarginPx
            || projected.x > (viewportWidth - kEdgeMarginPx)
            || projected.y < kEdgeMarginPx
            || projected.y > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        QVariantMap labelEntry;
        labelEntry.insert("x", projected.x);
        labelEntry.insert("y", projected.y);
        labelEntry.insert("text", QString::fromStdString(state.body.displayName));
        labels.push_back(labelEntry);
    }

    for (const auto& labelRef : m_constellationLabelRefs) {
        if (labelRef.first.empty() || labelRef.second.empty()) {
            continue;
        }

        if (!seenLabels.insert(labelRef.first).second) {
            continue;
        }

        double sumX = 0.0;
        double sumY = 0.0;
        int visiblePointCount = 0;
        for (const std::string& hipId : labelRef.second) {
            const auto* horizontal = findHorizontal(hipId);
            if (horizontal == nullptr) {
                continue;
            }

            const auto projected = m_projection->project(*horizontal, projectionParams);
            if (!projected.isVisible) {
                continue;
            }

            sumX += projected.x;
            sumY += projected.y;
            ++visiblePointCount;
        }

        if (visiblePointCount == 0) {
            continue;
        }

        const double labelX = sumX / static_cast<double>(visiblePointCount);
        const double labelY = sumY / static_cast<double>(visiblePointCount);
        if (
            labelX < kEdgeMarginPx
            || labelX > (viewportWidth - kEdgeMarginPx)
            || labelY < kEdgeMarginPx
            || labelY > (viewportHeight - kEdgeMarginPx)
        ) {
            continue;
        }

        QVariantMap labelEntry;
        labelEntry.insert("x", labelX);
        labelEntry.insert("y", labelY);
        labelEntry.insert("text", QString::fromStdString(labelRef.first));
        labels.push_back(labelEntry);
    }

    return labels;
}
