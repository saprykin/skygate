#include "SkyContextController.hpp"

#include "CatalogCoordinator.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkyContextControllerSupport.hpp"

#include <QLocale>
#include <QNetworkAccessManager>

#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/core/SystemTimeSource.hpp"
#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/StarCatalogFactory.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
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
        m_ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*m_starCatalog);
    }

    if (m_starCatalog != nullptr) {
        applyCatalog(std::move(m_starCatalog), "Bundled", false);
    } else {
        m_catalogStatusText = "Catalog: Unavailable";
    }

    m_projection = skygate::core::createProjection(m_projectionType);
    m_catalogUrlText = QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl);
    const skygate::core::SystemTimeSource systemTimeSource;
    m_skyContext.utcTime = systemTimeSource.nowUtc();
    loadSettings();

    m_timer.setInterval(SkyContextControllerConstants::kTickIntervalMs);
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
    return SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toString("HH:mm:ss");
}

QString SkyContextController::utcDateText() const
{
    return SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toString("yyyy-MM-dd");
}

QString SkyContextController::latitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_skyContext.observer.latitudeDeg);
}

QString SkyContextController::longitudeText() const
{
    return SkyContextTextFormatter::formatCoordinate(m_skyContext.observer.longitudeDeg);
}

QString SkyContextController::elevationText() const
{
    return SkyContextTextFormatter::formatElevation(m_skyContext.observer.elevationMeters);
}

QString SkyContextController::projectionTypeText() const
{
    return SkyContextProjectionTypeCodec::toString(m_projectionType);
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
        ? QString::fromUtf8(SkyContextControllerConstants::kHygCatalogPrimaryUrl)
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

    const skygate::core::ProjectionParams projectionParams = skygate::core::ViewportMath::buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );

    const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
    const SkyRenderPointBuilder renderPointBuilder;
    return renderPointBuilder.buildPoints(
        snapshot,
        *m_projection,
        projectionParams,
        m_magnitudeCutoff
    );
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

    const skygate::core::ProjectionParams projectionParams = skygate::core::ViewportMath::buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );

    const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
    const SkyConstellationRenderBuilder renderBuilder;
    return renderBuilder.buildLines(
        snapshot,
        *m_projection,
        projectionParams,
        m_constellationLineRefs,
        viewportWidth,
        viewportHeight
    );
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

    const skygate::core::ProjectionParams projectionParams = skygate::core::ViewportMath::buildProjectionParams(
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

    const skygate::core::ProjectionParams projectionParams = skygate::core::ViewportMath::buildProjectionParams(
        viewportWidth,
        viewportHeight,
        m_viewCenterAltitudeDeg,
        m_viewCenterAzimuthDeg,
        m_viewFieldOfViewDeg
    );

    const auto snapshot = m_ephemerisEngine->compute(m_skyContext);
    const SkyConstellationRenderBuilder renderBuilder;
    return renderBuilder.buildLabels(
        snapshot,
        *m_projection,
        projectionParams,
        m_constellationLabelRefs,
        viewportWidth,
        viewportHeight
    );
}
