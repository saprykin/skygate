#include "SkySceneModel.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QPointF>
#include <QVariantMap>

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/ProjectionMath.hpp"
#include "skygate/core/math/ViewportMath.hpp"

#include <array>
#include <cmath>
#include <limits>

namespace {

constexpr double kHoverLookupCellSizePx = 24.0;

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

QVariantMap overlayEntry(
    const QString& kind,
    const double x,
    const double y,
    const QString& text,
    const QColor& color
)
{
    QVariantMap entry;
    entry.insert("kind", kind);
    entry.insert("x", x);
    entry.insert("y", y);
    entry.insert("text", text);
    entry.insert("color", color);
    return entry;
}

QColor cardinalColor(const double azimuthDeg)
{
    if (azimuthDeg == 0.0) {
        return QColor(156, 231, 255);
    }
    if (azimuthDeg == 90.0) {
        return QColor(255, 207, 157);
    }
    if (azimuthDeg == 180.0) {
        return QColor(255, 176, 176);
    }
    return QColor(168, 233, 200);
}

QString normalizedLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

QString normalizedLookupKey(const std::string& value)
{
    return normalizedLookupKey(QString::fromStdString(value));
}

bool hasFiniteHorizontal(const skygate::core::HorizontalCoordinate& horizontal)
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

QVariantMap selectionMarkerEntry(
    const double x,
    const double y
)
{
    QVariantMap entry;
    entry.insert("kind", "searchSelection");
    entry.insert("x", x);
    entry.insert("y", y);
    return entry;
}

std::optional<QPointF> selectedBodyPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedLookupKey(targetId);
    const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedTargetId);
    if (stateIndexIt == stateIndexByBodyId.cend()) {
        return std::nullopt;
    }

    const auto& state = snapshot.states.at(*stateIndexIt);
    if (!hasFiniteHorizontal(state.horizontal)) {
        return std::nullopt;
    }

    const auto projected = preparedProjection.project(state.horizontal);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

std::optional<QPointF> selectedConstellationPoint(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QHash<QString, std::size_t>& stateIndexByBodyId,
    const skygate::core::PreparedProjection& preparedProjection,
    const std::span<const SkyContextController::ConstellationLabelRef> labelRefs,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedLookupKey(targetId);
    const auto labelRefIt = std::find_if(
        labelRefs.begin(),
        labelRefs.end(),
        [&normalizedTargetId](const SkyContextController::ConstellationLabelRef& labelRef) {
            return normalizedLookupKey(labelRef.first) == normalizedTargetId;
        }
    );
    if (labelRefIt == labelRefs.end()) {
        return std::nullopt;
    }

    skygate::core::ProjectionMath::Vec3 sum {0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : labelRefIt->second) {
        const auto stateIndexIt = stateIndexByBodyId.constFind(normalizedLookupKey(hipId));
        if (stateIndexIt == stateIndexByBodyId.cend()) {
            continue;
        }

        const auto& state = snapshot.states.at(*stateIndexIt);
        if (!hasFiniteHorizontal(state.horizontal)) {
            continue;
        }

        const auto vector = skygate::core::ProjectionMath::horizontalToUnitVector(
            state.horizontal
        );
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = skygate::core::ProjectionMath::normalize(sum);
    if (skygate::core::ProjectionMath::length(normalizedVector) <= 0.0) {
        return std::nullopt;
    }

    const skygate::core::HorizontalCoordinate center {
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(
            normalizedVector[2],
            -1.0,
            1.0
        ))),
        .azimuthDeg = skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AngleMath::toDegrees(
                std::atan2(normalizedVector[0], normalizedVector[1])
            )
        )
    };
    const auto projected = preparedProjection.project(center);
    if (!projected.isVisible) {
        return std::nullopt;
    }

    return QPointF(projected.x, projected.y);
}

}  // namespace

SkySceneModel::SkySceneModel(QObject* parent)
    : QObject(parent)
{
}

QObject* SkySceneModel::skyContextController() const noexcept
{
    return m_skyContextController;
}

void SkySceneModel::setSkyContextController(QObject* skyContextController)
{
    SkyContextController* controller = qobject_cast<SkyContextController*>(skyContextController);
    if (m_skyContextController == controller) {
        return;
    }

    disconnectFromContextController();
    m_skyContextController = controller;
    m_cachedEphemerisEngine = nullptr;
    m_snapshotCacheKey.reset();
    m_renderFrameKey.reset();
    m_snapshotGeneration = 0;
    m_sceneFrame = {};
    if (m_skyContextController != nullptr) {
        m_skyContextChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::skyContextChanged,
            this,
            &SkySceneModel::rebuildSceneFrame
        );
        m_selectedSearchTargetChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::selectedSearchTargetChanged,
            this,
            &SkySceneModel::rebuildSceneFrame
        );
    }

    emit skyContextControllerChanged();
    rebuildSceneFrame();
}

QVariantList SkySceneModel::overlayItems() const
{
    return m_sceneFrame.overlayItems;
}

QVariantMap SkySceneModel::selectionMarker() const
{
    return m_sceneFrame.selectionMarker;
}

std::uint64_t SkySceneModel::snapshotGeneration() const noexcept
{
    return m_snapshotGeneration;
}

void SkySceneModel::setViewportSize(const double viewportWidth, const double viewportHeight)
{
    if (
        std::abs(m_viewportWidth - viewportWidth) < 1e-9
        && std::abs(m_viewportHeight - viewportHeight) < 1e-9
    ) {
        return;
    }

    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    rebuildSceneFrame();
}

QString SkySceneModel::objectLabelAt(const double x, const double y) const
{
    if (
        m_viewportWidth <= 0.0
        || m_viewportHeight <= 0.0
        || m_sceneFrame.frame.points.empty()
        || m_sceneFrame.snapshot.catalogBodies == nullptr
    ) {
        return {};
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestPointIndex = m_sceneFrame.frame.points.size();
    const std::int32_t cellX = static_cast<std::int32_t>(std::floor(x / kHoverLookupCellSizePx));
    const std::int32_t cellY = static_cast<std::int32_t>(std::floor(y / kHoverLookupCellSizePx));

    for (int deltaY = -1; deltaY <= 1; ++deltaY) {
        for (int deltaX = -1; deltaX <= 1; ++deltaX) {
            const auto cellIt = m_sceneFrame.hoverPointIndicesByCell.find(
                (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX + deltaX)) << 32U)
                | static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellY + deltaY))
            );
            if (cellIt == m_sceneFrame.hoverPointIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t pointIndex : cellIt->second) {
                const auto& point = m_sceneFrame.frame.points[pointIndex];
                const auto& body = m_sceneFrame.snapshot.bodyAt(point.bodyIndex);
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

    if (bestPointIndex >= m_sceneFrame.frame.points.size()) {
        return {};
    }

    return QString::fromStdString(
        m_sceneFrame.snapshot.bodyAt(m_sceneFrame.frame.points[bestPointIndex].bodyIndex).displayName
    );
}

std::optional<skygate::core::PreparedProjection> SkySceneModel::preparedProjection() const
{
    return m_sceneFrame.preparedProjection;
}

std::span<const SkyRenderPoint> SkySceneModel::renderPointSpan() const
{
    return std::span<const SkyRenderPoint>(m_sceneFrame.frame.points);
}

std::span<const SkyRenderLine> SkySceneModel::renderLineSpan() const
{
    return std::span<const SkyRenderLine>(m_sceneFrame.frame.lines);
}

void SkySceneModel::disconnectFromContextController()
{
    if (m_skyContextChangedConnection) {
        disconnect(m_skyContextChangedConnection);
    }
    if (m_selectedSearchTargetChangedConnection) {
        disconnect(m_selectedSearchTargetChangedConnection);
    }

    m_skyContextChangedConnection = {};
    m_selectedSearchTargetChangedConnection = {};
}

bool SkySceneModel::clearSceneFrame()
{
    const bool hadSceneFrame = m_sceneFrame.preparedProjection.has_value()
        || m_sceneFrame.snapshot.catalogBodies != nullptr
        || !m_sceneFrame.frame.points.empty()
        || !m_sceneFrame.frame.lines.empty()
        || !m_sceneFrame.overlayItems.isEmpty()
        || !m_sceneFrame.selectionMarker.isEmpty();
    m_cachedEphemerisEngine = nullptr;
    m_snapshotCacheKey.reset();
    m_renderFrameKey.reset();
    m_snapshotGeneration = 0;
    m_sceneFrame = {};
    return hadSceneFrame;
}

void SkySceneModel::rebuildSceneFrame()
{
    if (
        m_skyContextController == nullptr
        || m_viewportWidth <= 0.0
        || m_viewportHeight <= 0.0
    ) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    const auto* ephemerisEngine = m_skyContextController->ephemerisEngine();
    if (ephemerisEngine == nullptr) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    bool sceneFrameUpdated = false;
    const skygate::core::ProjectionParams projectionParams =
        skygate::core::ViewportMath::buildProjectionParams(
            m_viewportWidth,
            m_viewportHeight,
            m_skyContextController->viewCenterAltitudeDeg(),
            m_skyContextController->viewCenterAzimuthDeg(),
            m_skyContextController->viewFieldOfViewDeg()
        );
    auto preparedProjection = skygate::core::PreparedProjection::create(
        m_skyContextController->projectionType(),
        projectionParams
    );
    if (!preparedProjection.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    const auto& skyContext = m_skyContextController->skyContext();
    const SnapshotCacheKey snapshotKey {
        .catalogRevision = m_skyContextController->catalogRevision(),
        .observer = skyContext.observer,
        .utcTime = skyContext.utcTime
    };
    if (
        m_cachedEphemerisEngine != ephemerisEngine
        || !m_snapshotCacheKey.has_value()
        || !m_snapshotCacheKey.value().equals(snapshotKey)
    ) {
        m_sceneFrame.snapshot = ephemerisEngine->compute(skyContext);
        m_sceneFrame.stateIndexByBodyId.clear();
        m_sceneFrame.stateIndexByBodyId.reserve(static_cast<qsizetype>(m_sceneFrame.snapshot.states.size()));
        for (std::size_t stateIndex = 0; stateIndex < m_sceneFrame.snapshot.states.size(); ++stateIndex) {
            const auto& state = m_sceneFrame.snapshot.states[stateIndex];
            const auto& body = m_sceneFrame.snapshot.bodyAt(state.bodyIndex);
            m_sceneFrame.stateIndexByBodyId.insert(normalizedLookupKey(body.id), stateIndex);
        }
        m_cachedEphemerisEngine = ephemerisEngine;
        m_snapshotCacheKey = snapshotKey;
        m_renderFrameKey.reset();
        ++m_snapshotGeneration;
        sceneFrameUpdated = true;
    }

    m_sceneFrame.preparedProjection = std::move(preparedProjection);

    const RenderFrameKey renderFrameKey {
        .snapshotGeneration = m_snapshotGeneration,
        .projectionType = m_skyContextController->projectionType(),
        .viewportWidth = m_viewportWidth,
        .viewportHeight = m_viewportHeight,
        .viewCenterAltitudeDeg = m_skyContextController->viewCenterAltitudeDeg(),
        .viewCenterAzimuthDeg = m_skyContextController->viewCenterAzimuthDeg(),
        .viewFieldOfViewDeg = m_skyContextController->viewFieldOfViewDeg(),
        .magnitudeCutoff = m_skyContextController->magnitudeCutoff()
    };
    if (!m_renderFrameKey.has_value() || !m_renderFrameKey.value().equals(renderFrameKey)) {
        const SkyRenderFrameBuilder frameBuilder;
        m_sceneFrame.frame = frameBuilder.buildFrame(
            m_sceneFrame.snapshot,
            *m_sceneFrame.preparedProjection,
            m_skyContextController->constellationLineRefs(),
            m_skyContextController->constellationLabelRefs(),
            m_skyContextController->magnitudeCutoff(),
            m_viewportWidth,
            m_viewportHeight
        );

        m_sceneFrame.hoverPointIndicesByCell.clear();
        m_sceneFrame.hoverPointIndicesByCell.reserve(
            (m_sceneFrame.frame.points.size() / 4U) + 1U
        );
        for (std::size_t pointIndex = 0; pointIndex < m_sceneFrame.frame.points.size(); ++pointIndex) {
            const auto& point = m_sceneFrame.frame.points[pointIndex];
            const auto& body = m_sceneFrame.snapshot.bodyAt(point.bodyIndex);
            if (body.displayName.empty()) {
                continue;
            }

            m_sceneFrame.hoverPointIndicesByCell[
                hoverCellKey(point.x, point.y, kHoverLookupCellSizePx)
            ].push_back(pointIndex);
        }

        m_sceneFrame.overlayItems = buildOverlayItems(m_sceneFrame);
        m_sceneFrame.selectionMarker = buildSelectionMarker(m_sceneFrame);
        m_renderFrameKey = renderFrameKey;
        sceneFrameUpdated = true;
    }

    if (sceneFrameUpdated) {
        emit sceneFrameChanged();
    }
}

QVariantList SkySceneModel::buildOverlayItems(const SceneFrameData& sceneFrame) const
{
    QVariantList overlayItems = sceneFrame.frame.labels;

    if (!sceneFrame.preparedProjection.has_value()) {
        return overlayItems;
    }

    constexpr std::array<const char*, 4> kCardinalLabels {"N", "E", "S", "W"};
    constexpr std::array<double, 4> kCardinalAzimuths {0.0, 90.0, 180.0, 270.0};
    for (std::size_t index = 0; index < kCardinalLabels.size(); ++index) {
        const auto projected = sceneFrame.preparedProjection->project(
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 0.0,
                .azimuthDeg = kCardinalAzimuths[index]
            }
        );
        if (!projected.isVisible) {
            continue;
        }

        overlayItems.push_back(overlayEntry(
            "cardinal",
            projected.x,
            projected.y,
            QString::fromUtf8(kCardinalLabels[index]),
            cardinalColor(kCardinalAzimuths[index])
        ));
    }

    return overlayItems;
}

QVariantMap SkySceneModel::buildSelectionMarker(const SceneFrameData& sceneFrame) const
{
    if (
        m_skyContextController == nullptr
        || !sceneFrame.preparedProjection.has_value()
    ) {
        return {};
    }

    const QString targetKind = m_skyContextController->selectedSearchTargetKind();
    const QString targetId = m_skyContextController->selectedSearchTargetId();
    if (targetKind.trimmed().isEmpty() || targetId.trimmed().isEmpty()) {
        return {};
    }

    std::optional<QPointF> markerPoint;
    if (normalizedLookupKey(targetKind) == "body") {
        markerPoint = selectedBodyPoint(
            sceneFrame.snapshot,
            sceneFrame.stateIndexByBodyId,
            *sceneFrame.preparedProjection,
            targetId
        );
    } else if (normalizedLookupKey(targetKind) == "constellationlabel") {
        markerPoint = selectedConstellationPoint(
            sceneFrame.snapshot,
            sceneFrame.stateIndexByBodyId,
            *sceneFrame.preparedProjection,
            m_skyContextController->constellationLabelRefs(),
            targetId
        );
    }

    if (!markerPoint.has_value()) {
        return {};
    }

    return selectionMarkerEntry(markerPoint->x(), markerPoint->y());
}
