#include "SkyContextController.hpp"

#include "CatalogCoordinator.hpp"
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
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>

using namespace skygate::ui::internal;

namespace {

constexpr double kHoverLookupCellSizePx = 24.0;

struct SnapshotCacheKey final {
    std::uint64_t catalogRevision = 0;
    skygate::core::UtcTimePoint utcTime {};
    skygate::core::GeoLocation observer;

    [[nodiscard]] bool matches(
        const std::uint64_t nextCatalogRevision,
        const skygate::core::SkyContext& context
    ) const noexcept
    {
        return catalogRevision == nextCatalogRevision
            && utcTime == context.utcTime
            && observer.latitudeDeg == context.observer.latitudeDeg
            && observer.longitudeDeg == context.observer.longitudeDeg
            && observer.elevationMeters == context.observer.elevationMeters;
    }
};

struct RenderFrameKey final {
    std::uint64_t snapshotGeneration = 0;
    skygate::core::ProjectionType projectionType = skygate::core::ProjectionType::Stereographic;
    std::int32_t viewportWidthPx = 0;
    std::int32_t viewportHeightPx = 0;
    double centerAltitudeDeg = 0.0;
    double centerAzimuthDeg = 0.0;
    double fieldOfViewDeg = 0.0;
    double magnitudeCutoff = 0.0;

    [[nodiscard]] bool matches(
        const std::uint64_t nextSnapshotGeneration,
        const skygate::core::ProjectionType nextProjectionType,
        const double viewportWidth,
        const double viewportHeight,
        const double nextCenterAltitudeDeg,
        const double nextCenterAzimuthDeg,
        const double nextFieldOfViewDeg,
        const double nextMagnitudeCutoff
    ) const noexcept
    {
        return snapshotGeneration == nextSnapshotGeneration
            && projectionType == nextProjectionType
            && viewportWidthPx == static_cast<std::int32_t>(std::lround(viewportWidth))
            && viewportHeightPx == static_cast<std::int32_t>(std::lround(viewportHeight))
            && centerAltitudeDeg == nextCenterAltitudeDeg
            && centerAzimuthDeg == nextCenterAzimuthDeg
            && fieldOfViewDeg == nextFieldOfViewDeg
            && magnitudeCutoff == nextMagnitudeCutoff;
    }
};

[[nodiscard]] std::uint64_t hoverCellKey(
    const double x,
    const double y,
    const double cellSizePx
) noexcept
{
    const std::uint32_t cellX = static_cast<std::uint32_t>(std::floor(x / cellSizePx));
    const std::uint32_t cellY = static_cast<std::uint32_t>(std::floor(y / cellSizePx));
    return (static_cast<std::uint64_t>(cellX) << 32U) | static_cast<std::uint64_t>(cellY);
}

}  // namespace

struct SkyContextController::RenderCacheState final {
    std::uint64_t cacheGeneration = 0;
    SnapshotCacheKey snapshotKey;
    bool snapshotValid = false;
    std::uint64_t snapshotGeneration = 0;
    skygate::ephemeris::SkySnapshot snapshot;

    RenderFrameKey frameKey;
    bool frameValid = false;
    SkyRenderFrame frame;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> hoverPointIndicesByCell;
};

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

std::vector<SkyRenderPoint> SkyContextController::renderPoints(
    const double viewportWidth,
    const double viewportHeight
) const
{
    const auto points = renderPointSpan(viewportWidth, viewportHeight);
    return std::vector<SkyRenderPoint>(points.begin(), points.end());
}

std::vector<SkyRenderLine> SkyContextController::renderConstellationLines(
    const double viewportWidth,
    const double viewportHeight
) const
{
    const auto lines = renderConstellationLineSpan(viewportWidth, viewportHeight);
    return std::vector<SkyRenderLine>(lines.begin(), lines.end());
}

std::span<const SkyRenderPoint> SkyContextController::renderPointSpan(
    const double viewportWidth,
    const double viewportHeight
) const
{
    return std::span<const SkyRenderPoint>(renderCache(viewportWidth, viewportHeight).frame.points);
}

std::span<const SkyRenderLine> SkyContextController::renderConstellationLineSpan(
    const double viewportWidth,
    const double viewportHeight
) const
{
    return std::span<const SkyRenderLine>(renderCache(viewportWidth, viewportHeight).frame.lines);
}

std::optional<skygate::core::PreparedProjection> SkyContextController::buildPreparedProjection(
    const double viewportWidth,
    const double viewportHeight
) const
{
    if (m_projection == nullptr || viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return std::nullopt;
    }

    const skygate::core::ProjectionParams projectionParams =
        skygate::core::ViewportMath::buildProjectionParams(
            viewportWidth,
            viewportHeight,
            m_viewCenterAltitudeDeg,
            m_viewCenterAzimuthDeg,
            m_viewFieldOfViewDeg
        );
    return skygate::core::PreparedProjection::create(m_projectionType, projectionParams);
}

skygate::core::ScreenPoint SkyContextController::projectHorizontal(
    const skygate::core::HorizontalCoordinate& coordinate,
    const double viewportWidth,
    const double viewportHeight
) const noexcept
{
    const auto preparedProjection = buildPreparedProjection(viewportWidth, viewportHeight);
    if (!preparedProjection.has_value()) {
        return {};
    }

    return preparedProjection->project(coordinate);
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
    if (viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }

    const auto& cache = renderCache(viewportWidth, viewportHeight);
    if (cache.frame.points.empty() || cache.snapshot.catalogBodies == nullptr) {
        return {};
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestPointIndex = cache.frame.points.size();
    const std::int32_t cellX = static_cast<std::int32_t>(std::floor(x / kHoverLookupCellSizePx));
    const std::int32_t cellY = static_cast<std::int32_t>(std::floor(y / kHoverLookupCellSizePx));

    for (int deltaY = -1; deltaY <= 1; ++deltaY) {
        for (int deltaX = -1; deltaX <= 1; ++deltaX) {
            const auto cellIt = cache.hoverPointIndicesByCell.find(
                (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX + deltaX)) << 32U)
                | static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellY + deltaY))
            );
            if (cellIt == cache.hoverPointIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t pointIndex : cellIt->second) {
                const auto& point = cache.frame.points[pointIndex];
                const auto& body = cache.snapshot.bodyAt(point.bodyIndex);
                if (body.displayName.empty()) {
                    continue;
                }

                const double deltaPointX = x - point.x;
                const double deltaPointY = y - point.y;
                const double distanceSquared =
                    (deltaPointX * deltaPointX) + (deltaPointY * deltaPointY);
                const double hitRadius = std::max(10.0, point.sizePx + 5.0);
                if (distanceSquared > (hitRadius * hitRadius)) {
                    continue;
                }

                if (distanceSquared < bestDistanceSquared) {
                    bestDistanceSquared = distanceSquared;
                    bestPointIndex = pointIndex;
                }
            }
        }
    }

    if (bestPointIndex >= cache.frame.points.size()) {
        return {};
    }

    return QString::fromStdString(
        cache.snapshot.bodyAt(cache.frame.points[bestPointIndex].bodyIndex).displayName
    );
}

QVariantList SkyContextController::constellationLabels(
    const double viewportWidth,
    const double viewportHeight
) const
{
    return renderCache(viewportWidth, viewportHeight).frame.labels;
}

const SkyContextController::RenderCacheState& SkyContextController::renderCache(
    const double viewportWidth,
    const double viewportHeight
) const
{
    static thread_local std::unordered_map<
        const SkyContextController*,
        std::unique_ptr<RenderCacheState>
    > cachesByController;

    auto& cacheEntry = cachesByController[this];
    if (cacheEntry == nullptr) {
        cacheEntry = std::make_unique<RenderCacheState>();
    }

    auto& cache = *cacheEntry;
    const std::uint64_t cacheGeneration = m_renderCacheGeneration.load(std::memory_order_relaxed);
    if (cache.cacheGeneration != cacheGeneration) {
        cache = {};
        cache.cacheGeneration = cacheGeneration;
    }

    const std::uint64_t catalogRevision = m_catalogRevision.load(std::memory_order_relaxed);
    if (m_ephemerisEngine != nullptr) {
        if (!cache.snapshotKey.matches(catalogRevision, m_skyContext) || !cache.snapshotValid) {
            cache.snapshot = m_ephemerisEngine->compute(m_skyContext);
            cache.snapshotKey.catalogRevision = catalogRevision;
            cache.snapshotKey.utcTime = m_skyContext.utcTime;
            cache.snapshotKey.observer = m_skyContext.observer;
            cache.snapshotValid = true;
            ++cache.snapshotGeneration;
            cache.frameValid = false;
        }
    } else if (!cache.snapshotValid) {
        cache.snapshot = {};
        cache.snapshotValid = true;
        ++cache.snapshotGeneration;
        cache.frameValid = false;
    }

    if (
        cache.frameValid
        && cache.frameKey.matches(
            cache.snapshotGeneration,
            m_projectionType,
            viewportWidth,
            viewportHeight,
            m_viewCenterAltitudeDeg,
            m_viewCenterAzimuthDeg,
            m_viewFieldOfViewDeg,
            m_magnitudeCutoff
        )
    ) {
        return cache;
    }

    cache.frame = {};
    cache.hoverPointIndicesByCell.clear();
    if (const auto preparedProjection = buildPreparedProjection(viewportWidth, viewportHeight);
        preparedProjection.has_value()) {
        const SkyRenderFrameBuilder frameBuilder;
        cache.frame = frameBuilder.buildFrame(
            cache.snapshot,
            *preparedProjection,
            m_constellationLineRefs,
            m_constellationLabelRefs,
            m_magnitudeCutoff,
            viewportWidth,
            viewportHeight
        );

        cache.hoverPointIndicesByCell.reserve((cache.frame.points.size() / 4U) + 1U);
        for (std::size_t pointIndex = 0; pointIndex < cache.frame.points.size(); ++pointIndex) {
            const auto& point = cache.frame.points[pointIndex];
            const auto& body = cache.snapshot.bodyAt(point.bodyIndex);
            if (body.displayName.empty()) {
                continue;
            }

            cache.hoverPointIndicesByCell[hoverCellKey(point.x, point.y, kHoverLookupCellSizePx)]
                .push_back(pointIndex);
        }
    }

    cache.frameKey.snapshotGeneration = cache.snapshotGeneration;
    cache.frameKey.projectionType = m_projectionType;
    cache.frameKey.viewportWidthPx = static_cast<std::int32_t>(std::lround(viewportWidth));
    cache.frameKey.viewportHeightPx = static_cast<std::int32_t>(std::lround(viewportHeight));
    cache.frameKey.centerAltitudeDeg = m_viewCenterAltitudeDeg;
    cache.frameKey.centerAzimuthDeg = m_viewCenterAzimuthDeg;
    cache.frameKey.fieldOfViewDeg = m_viewFieldOfViewDeg;
    cache.frameKey.magnitudeCutoff = m_magnitudeCutoff;
    cache.frameValid = true;
    return cache;
}

void SkyContextController::invalidateRenderCaches() noexcept
{
    m_renderCacheGeneration.fetch_add(1U, std::memory_order_relaxed);
}
