#include "SkySceneModel.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QPointF>
#include <QVariantMap>

#include "skygate/core/math/GeometryMath.hpp"
#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CelestialReferenceCalculator.hpp"
#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include <array>
#include <cmath>
#include <limits>

namespace {

constexpr double kHoverLookupCellSizePx = 24.0;
constexpr int kReferenceLabelSampleCount = 96;
constexpr double kReferenceLabelEdgeMarginPx = 36.0;

[[nodiscard]] std::uint64_t hoverCellKey(
    const double x,
    const double y,
    const double cellSizePx
) noexcept
{
    return skygate::core::GeometryMath::packedGridCellKey(x, y, cellSizePx);
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

QColor cardinalColor(
    const double azimuthDeg,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    if (azimuthDeg == 0.0) {
        return renderTheme.cardinalNorthLine;
    }
    if (azimuthDeg == 90.0) {
        return renderTheme.cardinalEastLine;
    }
    if (azimuthDeg == 180.0) {
        return renderTheme.cardinalSouthLine;
    }
    return renderTheme.cardinalWestLine;
}

bool pointIsInsideLabelMargin(
    const skygate::core::ScreenPoint& point,
    const skygate::core::ProjectionParams& params
) noexcept
{
    return point.x >= kReferenceLabelEdgeMarginPx
        && point.x <= (params.viewportWidth - kReferenceLabelEdgeMarginPx)
        && point.y >= kReferenceLabelEdgeMarginPx
        && point.y <= (params.viewportHeight - kReferenceLabelEdgeMarginPx);
}

template <typename CoordinateAt>
void appendReferenceLayerLabel(
    QVariantList& overlayItems,
    const skygate::core::PreparedProjection& preparedProjection,
    const QString& text,
    const QColor& color,
    CoordinateAt coordinateAt
)
{
    const auto& params = preparedProjection.params();
    const double targetX = params.viewportWidth * 0.72;
    const double targetY = params.viewportHeight * 0.34;

    std::optional<QPointF> bestPoint;
    double bestScore = std::numeric_limits<double>::max();
    std::optional<QPointF> fallbackPoint;
    double fallbackScore = std::numeric_limits<double>::max();

    for (int index = 0; index < kReferenceLabelSampleCount; ++index) {
        const auto projected = preparedProjection.project(coordinateAt(index));
        if (!projected.isVisible) {
            continue;
        }

        const double score = skygate::core::GeometryMath::squaredDistance2d(
            projected.x,
            projected.y,
            targetX,
            targetY
        );
        if (score < fallbackScore) {
            fallbackPoint = QPointF(projected.x, projected.y);
            fallbackScore = score;
        }
        if (pointIsInsideLabelMargin(projected, params) && score < bestScore) {
            bestPoint = QPointF(projected.x, projected.y);
            bestScore = score;
        }
    }

    const std::optional<QPointF>& labelPoint = bestPoint.has_value() ? bestPoint : fallbackPoint;
    if (!labelPoint.has_value()) {
        return;
    }

    overlayItems.push_back(overlayEntry(
        "referenceLine",
        labelPoint->x(),
        labelPoint->y(),
        text,
        color
    ));
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
    const skygate::core::PreparedProjection& preparedProjection,
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs,
    const QString& targetId
)
{
    const auto center = skygate::ephemeris::ConstellationReferenceCalculator::labelCenter(
        snapshot,
        labelRefs,
        targetId.toStdString()
    );
    if (!center.has_value()) {
        return std::nullopt;
    }

    const auto projected = preparedProjection.project(*center);
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
        m_themeChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::themeChanged,
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
        || m_sceneFrame.hoverTargets.empty()
        || m_sceneFrame.snapshot.catalogBodies == nullptr
    ) {
        return {};
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::size_t bestTargetIndex = m_sceneFrame.hoverTargets.size();
    const std::int32_t cellX =
        skygate::core::GeometryMath::gridCellIndex(x, kHoverLookupCellSizePx);
    const std::int32_t cellY =
        skygate::core::GeometryMath::gridCellIndex(y, kHoverLookupCellSizePx);

    for (int deltaY = -1; deltaY <= 1; ++deltaY) {
        for (int deltaX = -1; deltaX <= 1; ++deltaX) {
            const auto cellIt = m_sceneFrame.hoverTargetIndicesByCell.find(
                skygate::core::GeometryMath::packedGridCellKey(cellX + deltaX, cellY + deltaY)
            );
            if (cellIt == m_sceneFrame.hoverTargetIndicesByCell.end()) {
                continue;
            }

            for (const std::size_t targetIndex : cellIt->second) {
                const auto& target = m_sceneFrame.hoverTargets[targetIndex];
                const auto& body = m_sceneFrame.snapshot.bodyAt(target.bodyIndex);
                if (body.displayName.empty()) {
                    continue;
                }

                const double distanceSquared = skygate::core::GeometryMath::squaredDistance2d(
                    x,
                    y,
                    target.x,
                    target.y
                );
                const double hitRadius = target.radiusPx;
                if (distanceSquared > (hitRadius * hitRadius)) {
                    continue;
                }

                if (distanceSquared < bestDistanceSquared) {
                    bestDistanceSquared = distanceSquared;
                    bestTargetIndex = targetIndex;
                }
            }
        }
    }

    if (bestTargetIndex >= m_sceneFrame.hoverTargets.size()) {
        return {};
    }

    const auto& body = m_sceneFrame.snapshot.bodyAt(
        m_sceneFrame.hoverTargets[bestTargetIndex].bodyIndex
    );
    return QString::fromStdString(body.displayName);
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

std::span<const SkyRenderGlyph> SkySceneModel::renderGlyphSpan() const
{
    return std::span<const SkyRenderGlyph>(m_sceneFrame.frame.glyphs);
}

void SkySceneModel::disconnectFromContextController()
{
    if (m_skyContextChangedConnection) {
        disconnect(m_skyContextChangedConnection);
    }
    if (m_selectedSearchTargetChangedConnection) {
        disconnect(m_selectedSearchTargetChangedConnection);
    }
    if (m_themeChangedConnection) {
        disconnect(m_themeChangedConnection);
    }

    m_skyContextChangedConnection = {};
    m_selectedSearchTargetChangedConnection = {};
    m_themeChangedConnection = {};
}

bool SkySceneModel::clearSceneFrame()
{
    const bool hadSceneFrame = m_sceneFrame.preparedProjection.has_value()
        || m_sceneFrame.snapshot.catalogBodies != nullptr
        || !m_sceneFrame.frame.points.empty()
        || !m_sceneFrame.frame.lines.empty()
        || !m_sceneFrame.frame.glyphs.empty()
        || !m_sceneFrame.overlayItems.isEmpty()
        || !m_sceneFrame.selectionMarker.isEmpty();
    m_cachedEphemerisEngine = nullptr;
    m_snapshotCacheKey.reset();
    m_renderFrameKey.reset();
    m_snapshotGeneration = 0;
    m_sceneFrame = {};
    return hadSceneFrame;
}

std::optional<SkySceneModel::SceneBuildInput> SkySceneModel::buildSceneInput() const
{
    if (
        m_skyContextController == nullptr
        || m_viewportWidth <= 0.0
        || m_viewportHeight <= 0.0
    ) {
        return std::nullopt;
    }

    const auto* ephemerisEngine = m_skyContextController->ephemerisEngine();
    if (ephemerisEngine == nullptr) {
        return std::nullopt;
    }

    return SceneBuildInput {
        .ephemerisEngine = ephemerisEngine,
        .skyContext = m_skyContextController->skyContext(),
        .catalogRevision = m_skyContextController->catalogRevision(),
        .projectionType = m_skyContextController->projectionType(),
        .viewCenterAltitudeDeg = m_skyContextController->viewCenterAltitudeDeg(),
        .viewCenterAzimuthDeg = m_skyContextController->viewCenterAzimuthDeg(),
        .viewFieldOfViewDeg = m_skyContextController->viewFieldOfViewDeg(),
        .magnitudeCutoff = m_skyContextController->magnitudeCutoff(),
        .themeId = m_skyContextController->themeId(),
        .renderTheme = m_skyContextController->renderTheme(),
        .overlayLayers = m_skyContextController->overlayLayerVisibility(),
        .constellationLineRefs = m_skyContextController->constellationLineRefs(),
        .constellationLabelRefs = m_skyContextController->constellationLabelRefs(),
        .selectedSearchTargetKind = m_skyContextController->selectedSearchTargetKind(),
        .selectedSearchTargetId = m_skyContextController->selectedSearchTargetId()
    };
}

void SkySceneModel::rebuildSceneFrame()
{
    const auto input = buildSceneInput();
    if (!input.has_value()) {
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
            input->viewCenterAltitudeDeg,
            input->viewCenterAzimuthDeg,
            input->viewFieldOfViewDeg
        );
    auto preparedProjection = skygate::core::PreparedProjection::create(
        input->projectionType,
        projectionParams
    );
    if (!preparedProjection.has_value()) {
        if (clearSceneFrame()) {
            emit sceneFrameChanged();
        }
        return;
    }

    const SnapshotCacheKey snapshotKey {
        .catalogRevision = input->catalogRevision,
        .observer = input->skyContext.observer,
        .utcTime = input->skyContext.utcTime
    };
    if (
        m_cachedEphemerisEngine != input->ephemerisEngine
        || !m_snapshotCacheKey.has_value()
        || !m_snapshotCacheKey.value().equals(snapshotKey)
    ) {
        m_sceneFrame.snapshot = input->ephemerisEngine->compute(input->skyContext);
        m_sceneFrame.stateIndexByBodyId.clear();
        m_sceneFrame.stateIndexByBodyId.reserve(
            static_cast<qsizetype>(m_sceneFrame.snapshot.states.size())
        );
        for (
            std::size_t stateIndex = 0;
            stateIndex < m_sceneFrame.snapshot.states.size();
            ++stateIndex
        ) {
            const auto& state = m_sceneFrame.snapshot.states[stateIndex];
            const auto& body = m_sceneFrame.snapshot.bodyAt(state.bodyIndex);
            m_sceneFrame.stateIndexByBodyId.insert(normalizedLookupKey(body.id), stateIndex);
        }
        m_cachedEphemerisEngine = input->ephemerisEngine;
        m_snapshotCacheKey = snapshotKey;
        m_renderFrameKey.reset();
        ++m_snapshotGeneration;
        sceneFrameUpdated = true;
    }

    m_sceneFrame.preparedProjection = std::move(preparedProjection);

    const RenderFrameKey renderFrameKey {
        .snapshotGeneration = m_snapshotGeneration,
        .projectionType = input->projectionType,
        .viewportWidth = m_viewportWidth,
        .viewportHeight = m_viewportHeight,
        .viewCenterAltitudeDeg = input->viewCenterAltitudeDeg,
        .viewCenterAzimuthDeg = input->viewCenterAzimuthDeg,
        .viewFieldOfViewDeg = input->viewFieldOfViewDeg,
        .magnitudeCutoff = input->magnitudeCutoff,
        .themeId = input->themeId,
        .overlayLayers = input->overlayLayers
    };
    if (!m_renderFrameKey.has_value() || !m_renderFrameKey.value().equals(renderFrameKey)) {
        const SkyRenderFrameBuilder frameBuilder;
        m_sceneFrame.frame = frameBuilder.buildFrame(
            m_sceneFrame.snapshot,
            *m_sceneFrame.preparedProjection,
            input->constellationLineRefs,
            input->constellationLabelRefs,
            input->magnitudeCutoff,
            m_viewportWidth,
            m_viewportHeight,
            input->renderTheme,
            input->overlayLayers
        );

        m_sceneFrame.hoverTargets.clear();
        m_sceneFrame.hoverTargets.reserve(
            m_sceneFrame.frame.points.size() + m_sceneFrame.frame.glyphs.size()
        );
        m_sceneFrame.hoverTargetIndicesByCell.clear();
        m_sceneFrame.hoverTargetIndicesByCell.reserve(
            ((m_sceneFrame.frame.points.size() + m_sceneFrame.frame.glyphs.size()) / 4U) + 1U
        );
        for (const auto& point : m_sceneFrame.frame.points) {
            const auto& body = m_sceneFrame.snapshot.bodyAt(point.bodyIndex);
            if (body.displayName.empty()) {
                continue;
            }

            const std::size_t targetIndex = m_sceneFrame.hoverTargets.size();
            const double hitRadius = std::max(10.0, point.sizePx + 5.0);
            m_sceneFrame.hoverTargets.push_back(SceneFrameData::HoverTarget {
                .bodyIndex = point.bodyIndex,
                .x = point.x,
                .y = point.y,
                .radiusPx = hitRadius
            });
            m_sceneFrame.hoverTargetIndicesByCell[
                hoverCellKey(point.x, point.y, kHoverLookupCellSizePx)
            ].push_back(targetIndex);
        }

        for (const auto& glyph : m_sceneFrame.frame.glyphs) {
            const auto& body = m_sceneFrame.snapshot.bodyAt(glyph.bodyIndex);
            if (body.displayName.empty()) {
                continue;
            }

            const double hitRadius = std::max({
                glyph.radiusXPx,
                glyph.radiusYPx,
                12.0
            });
            const std::size_t targetIndex = m_sceneFrame.hoverTargets.size();
            m_sceneFrame.hoverTargets.push_back(SceneFrameData::HoverTarget {
                .bodyIndex = glyph.bodyIndex,
                .x = glyph.x,
                .y = glyph.y,
                .radiusPx = hitRadius
            });
            m_sceneFrame.hoverTargetIndicesByCell[
                hoverCellKey(glyph.x, glyph.y, kHoverLookupCellSizePx)
            ].push_back(targetIndex);
        }

        m_sceneFrame.overlayItems = buildOverlayItems(m_sceneFrame, input.value());
        m_sceneFrame.selectionMarker = buildSelectionMarker(m_sceneFrame, input.value());
        m_renderFrameKey = renderFrameKey;
        sceneFrameUpdated = true;
    }

    if (sceneFrameUpdated) {
        emit sceneFrameChanged();
    }
}

QVariantList SkySceneModel::buildOverlayItems(
    const SceneFrameData& sceneFrame,
    const SceneBuildInput& input
) const
{
    QVariantList overlayItems = sceneFrame.frame.labels;

    if (!sceneFrame.preparedProjection.has_value()) {
        return overlayItems;
    }

    const auto& overlayLayers = input.overlayLayers;
    const auto& renderTheme = input.renderTheme;
    const auto& skyContext = input.skyContext;

    if (normalizedLookupKey(input.selectedSearchTargetKind) == "body") {
        const QString targetId = input.selectedSearchTargetId;
        const auto stateIndexIt = sceneFrame.stateIndexByBodyId.constFind(
            normalizedLookupKey(targetId)
        );
        if (stateIndexIt != sceneFrame.stateIndexByBodyId.cend()) {
            const auto& state = sceneFrame.snapshot.states.at(*stateIndexIt);
            const auto& body = sceneFrame.snapshot.bodyAt(state.bodyIndex);
            if (
                body.type == skygate::ephemeris::CelestialBodyType::DeepSkyObject
                && overlayLayers.deepSkyObjects
                && !body.displayName.empty()
                && hasFiniteHorizontal(state.horizontal)
            ) {
                const auto projected = sceneFrame.preparedProjection->project(state.horizontal);
                if (projected.isVisible) {
                    overlayItems.push_back(overlayEntry(
                        "selectionLabel",
                        projected.x,
                        projected.y,
                        QString::fromStdString(body.displayName),
                        renderTheme.labelDeepSkyObject
                    ));
                }
            }
        }
    }

    if (overlayLayers.ecliptic) {
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Ecliptic",
            renderTheme.eclipticLine,
            [&skyContext](const int index) {
                const double eclipticLongitudeDeg = 360.0 * static_cast<double>(index)
                    / static_cast<double>(kReferenceLabelSampleCount);
                return skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
                    eclipticLongitudeDeg,
                    skyContext.observer,
                    skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.celestialEquator) {
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Celestial equator",
            renderTheme.celestialEquatorLine,
            [&skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index,
                    kReferenceLabelSampleCount,
                    0.0,
                    skyContext.observer,
                    skyContext.utcTime
                );
            }
        );
    }

    if (overlayLayers.circumpolarBoundary && skyContext.observer.isValid()) {
        const double boundaryDeclinationDeg =
            skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
                skyContext.observer
            );
        appendReferenceLayerLabel(
            overlayItems,
            *sceneFrame.preparedProjection,
            "Circumpolar",
            renderTheme.circumpolarBoundaryLine,
            [boundaryDeclinationDeg, &skyContext](const int index) {
                return skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
                    index,
                    kReferenceLabelSampleCount,
                    boundaryDeclinationDeg,
                    skyContext.observer,
                    skyContext.utcTime
                );
            }
        );
    }

    if (!overlayLayers.horizon) {
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
            cardinalColor(kCardinalAzimuths[index], renderTheme)
        ));
    }

    return overlayItems;
}

QVariantMap SkySceneModel::buildSelectionMarker(
    const SceneFrameData& sceneFrame,
    const SceneBuildInput& input
) const
{
    if (!sceneFrame.preparedProjection.has_value()) {
        return {};
    }

    const QString targetKind = input.selectedSearchTargetKind;
    const QString targetId = input.selectedSearchTargetId;
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
            *sceneFrame.preparedProjection,
            input.constellationLabelRefs,
            targetId
        );
    }

    if (!markerPoint.has_value()) {
        return {};
    }

    return selectionMarkerEntry(markerPoint->x(), markerPoint->y());
}
