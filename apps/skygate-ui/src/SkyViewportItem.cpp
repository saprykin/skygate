#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {
constexpr int kHorizonSampleCount = 96;
constexpr int kGridAltitudeStepDeg = 15;
constexpr int kGridAzimuthStepDeg = 30;
constexpr int kGridAltitudeSampleCount = 96;
constexpr int kGridAzimuthSampleCount = 64;
constexpr float kGridLineWidthPx = 1.0F;
constexpr float kCardinalLineWidthPx = 1.8F;
constexpr float kHorizonLineWidthPx = 2.2F;
struct LineSegment final {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
    float widthPx = 1.0F;
    QColor color;
};

struct VertexBucket final {
    QColor color;
    float widthPx = 1.0F;
    std::vector<float> coordinates;
};

class SkySceneRootNode final : public QSGNode {
public:
    SkySceneRootNode()
    {
        m_lineRoot = new QSGNode();
        m_pointRoot = new QSGNode();
        appendChildNode(m_lineRoot);
        appendChildNode(m_pointRoot);
    }

    [[nodiscard]] QSGNode* lineRoot() const noexcept
    {
        return m_lineRoot;
    }

    [[nodiscard]] QSGNode* pointRoot() const noexcept
    {
        return m_pointRoot;
    }

private:
    QSGNode* m_lineRoot = nullptr;
    QSGNode* m_pointRoot = nullptr;
};

[[nodiscard]] QColor cardinalMeridianColor(
    const int azimuthDeg,
    const skygate::ui::internal::SkyThemeRenderPalette& renderTheme
)
{
    switch (azimuthDeg) {
    case 0:
        return renderTheme.cardinalNorthLine;
    case 90:
        return renderTheme.cardinalEastLine;
    case 180:
        return renderTheme.cardinalSouthLine;
    case 270:
        return renderTheme.cardinalWestLine;
    default:
        return renderTheme.gridAzimuthLine;
    }
}

void appendLineSegment(
    std::vector<LineSegment>& lineSegments,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const float widthPx,
    const QColor& color
)
{
    lineSegments.push_back(LineSegment {
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .widthPx = widthPx,
        .color = color
    });
}

void clearChildNodes(QSGNode* rootNode)
{
    if (rootNode == nullptr) {
        return;
    }

    QSGNode* child = rootNode->firstChild();
    while (child != nullptr) {
        QSGNode* nextChild = child->nextSibling();
        rootNode->removeChildNode(child);
        delete child;
        child = nextChild;
    }
}

void syncBatchedLineNodes(
    QSGNode* rootNode,
    const std::vector<LineSegment>& lineSegments
)
{
    if (rootNode == nullptr) {
        return;
    }

    std::map<std::uint32_t, VertexBucket> buckets;
    for (const auto& lineSegment : lineSegments) {
        const std::uint32_t colorKey = static_cast<std::uint32_t>(lineSegment.color.rgba());
        const std::uint32_t widthKey = static_cast<std::uint32_t>(lineSegment.widthPx * 100.0F);
        const std::uint32_t bucketKey = (colorKey << 8U) ^ widthKey;
        auto& bucket = buckets[bucketKey];
        bucket.color = lineSegment.color;
        bucket.widthPx = lineSegment.widthPx;

        const float deltaX = lineSegment.x2 - lineSegment.x1;
        const float deltaY = lineSegment.y2 - lineSegment.y1;
        const float length = std::hypot(deltaX, deltaY);
        if (length <= 0.0F) {
            continue;
        }

        const float halfWidth = lineSegment.widthPx * 0.5F;
        const float offsetX = -deltaY / length * halfWidth;
        const float offsetY = deltaX / length * halfWidth;

        const float startLeftX = lineSegment.x1 + offsetX;
        const float startLeftY = lineSegment.y1 + offsetY;
        const float startRightX = lineSegment.x1 - offsetX;
        const float startRightY = lineSegment.y1 - offsetY;
        const float endLeftX = lineSegment.x2 + offsetX;
        const float endLeftY = lineSegment.y2 + offsetY;
        const float endRightX = lineSegment.x2 - offsetX;
        const float endRightY = lineSegment.y2 - offsetY;

        bucket.coordinates.push_back(startLeftX);
        bucket.coordinates.push_back(startLeftY);
        bucket.coordinates.push_back(startRightX);
        bucket.coordinates.push_back(startRightY);
        bucket.coordinates.push_back(endLeftX);
        bucket.coordinates.push_back(endLeftY);

        bucket.coordinates.push_back(startRightX);
        bucket.coordinates.push_back(startRightY);
        bucket.coordinates.push_back(endRightX);
        bucket.coordinates.push_back(endRightY);
        bucket.coordinates.push_back(endLeftX);
        bucket.coordinates.push_back(endLeftY);
    }

    for (const auto& [bucketKey, bucket] : buckets) {
        (void) bucketKey;

        const int vertexCount = static_cast<int>(bucket.coordinates.size() / 2U);
        if (vertexCount <= 0) {
            continue;
        }

        QSGGeometryNode* node = new QSGGeometryNode();
        QSGGeometry* geometry = new QSGGeometry(
            QSGGeometry::defaultAttributes_Point2D(),
            vertexCount
        );
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);

        auto* vertices = geometry->vertexDataAsPoint2D();
        for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t coordinateOffset = static_cast<std::size_t>(vertexIndex) * 2U;
            vertices[vertexIndex].set(
                bucket.coordinates[coordinateOffset],
                bucket.coordinates[coordinateOffset + 1U]
            );
        }

        QSGFlatColorMaterial* material = new QSGFlatColorMaterial();
        material->setColor(bucket.color);

        node->setGeometry(geometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsGeometry, true);
        node->setFlag(QSGNode::OwnsMaterial, true);
        rootNode->appendChildNode(node);
    }
}

void syncBatchedPointNodes(
    QSGNode* rootNode,
    const std::span<const SkyRenderPoint> points
)
{
    if (rootNode == nullptr) {
        return;
    }

    std::map<std::uint32_t, VertexBucket> buckets;
    for (const auto& point : points) {
        const std::uint32_t colorKey = static_cast<std::uint32_t>(point.color.rgba());
        auto& bucket = buckets[colorKey];
        bucket.color = point.color;

        const float halfSize = static_cast<float>(point.sizePx * 0.5);
        const float left = static_cast<float>(point.x) - halfSize;
        const float right = static_cast<float>(point.x) + halfSize;
        const float top = static_cast<float>(point.y) - halfSize;
        const float bottom = static_cast<float>(point.y) + halfSize;

        bucket.coordinates.push_back(left);
        bucket.coordinates.push_back(top);
        bucket.coordinates.push_back(right);
        bucket.coordinates.push_back(top);
        bucket.coordinates.push_back(left);
        bucket.coordinates.push_back(bottom);

        bucket.coordinates.push_back(left);
        bucket.coordinates.push_back(bottom);
        bucket.coordinates.push_back(right);
        bucket.coordinates.push_back(top);
        bucket.coordinates.push_back(right);
        bucket.coordinates.push_back(bottom);
    }

    for (const auto& [colorKey, bucket] : buckets) {
        (void) colorKey;

        const int vertexCount = static_cast<int>(bucket.coordinates.size() / 2U);
        if (vertexCount <= 0) {
            continue;
        }

        QSGGeometryNode* node = new QSGGeometryNode();
        QSGGeometry* geometry = new QSGGeometry(
            QSGGeometry::defaultAttributes_Point2D(),
            vertexCount
        );
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);

        auto* vertices = geometry->vertexDataAsPoint2D();
        for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            const std::size_t coordinateOffset = static_cast<std::size_t>(vertexIndex) * 2U;
            vertices[vertexIndex].set(
                bucket.coordinates[coordinateOffset],
                bucket.coordinates[coordinateOffset + 1U]
            );
        }

        QSGFlatColorMaterial* material = new QSGFlatColorMaterial();
        material->setColor(bucket.color);

        node->setGeometry(geometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsGeometry, true);
        node->setFlag(QSGNode::OwnsMaterial, true);
        rootNode->appendChildNode(node);
    }
}

template <typename CoordinateFn>
void appendProjectedPolyline(
    std::vector<LineSegment>& lineSegments,
    const skygate::core::PreparedProjection& projection,
    const int sampleCount,
    const double maxSegmentLengthSquared,
    const QColor& color,
    const float widthPx,
    CoordinateFn coordinateFn
)
{
    if (sampleCount <= 0) {
        return;
    }

    bool hasPreviousPoint = false;
    auto previousPoint = projection.project(coordinateFn(0));

    for (int index = 0; index <= sampleCount; ++index) {
        const auto projected = projection.project(coordinateFn(index));
        if (!projected.isVisible) {
            hasPreviousPoint = false;
            continue;
        }

        if (hasPreviousPoint) {
            const double deltaX = projected.x - previousPoint.x;
            const double deltaY = projected.y - previousPoint.y;
            const double segmentLengthSquared = (deltaX * deltaX) + (deltaY * deltaY);
            if (segmentLengthSquared <= maxSegmentLengthSquared) {
                appendLineSegment(
                    lineSegments,
                    static_cast<float>(previousPoint.x),
                    static_cast<float>(previousPoint.y),
                    static_cast<float>(projected.x),
                    static_cast<float>(projected.y),
                    widthPx,
                    color
                );
            }
        }

        previousPoint = projected;
        hasPreviousPoint = true;
    }
}

}  // namespace

struct SkyViewportItem::ViewportRenderData final {
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    std::vector<SkyRenderLine> lines;
    std::vector<SkyRenderPoint> points;
};

SkyViewportItem::SkyViewportItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

QObject* SkyViewportItem::skySceneModel() const noexcept
{
    return m_skySceneModel;
}

void SkyViewportItem::setSkySceneModel(QObject* skySceneModel)
{
    SkySceneModel* model = qobject_cast<SkySceneModel*>(skySceneModel);
    if (m_skySceneModel == model) {
        return;
    }

    disconnectFromSceneModel();
    m_skySceneModel = model;

    if (m_skySceneModel != nullptr) {
        m_sceneFrameChangedConnection = connect(
            m_skySceneModel,
            &SkySceneModel::sceneFrameChanged,
            this,
            &SkyViewportItem::synchronizeRenderData
        );
        m_skySceneModel->setViewportSize(width(), height());
    }

    emit skySceneModelChanged();
    synchronizeRenderData();
}

QSGNode* SkyViewportItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData)
{
    (void)updatePaintNodeData;

    auto* rootNode = static_cast<SkySceneRootNode*>(oldNode);
    if (rootNode == nullptr) {
        rootNode = new SkySceneRootNode();
    }

    std::shared_ptr<const ViewportRenderData> renderData;
    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        renderData = m_renderData;
    }

    if (renderData == nullptr || !renderData->preparedProjection.has_value()) {
        clearChildNodes(rootNode->lineRoot());
        clearChildNodes(rootNode->pointRoot());
        syncBatchedLineNodes(rootNode->lineRoot(), {});
        syncBatchedPointNodes(rootNode->pointRoot(), {});
        return rootNode;
    }

    const double viewportWidth = renderData->viewportWidth;
    const double viewportHeight = renderData->viewportHeight;
    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.30;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    std::vector<LineSegment> lineSegments;
    lineSegments.reserve(4096U);
    clearChildNodes(rootNode->lineRoot());
    clearChildNodes(rootNode->pointRoot());

    for (int altitudeDeg = -75; altitudeDeg <= 75; altitudeDeg += kGridAltitudeStepDeg) {
        if (altitudeDeg == 0) {
            continue;
        }

        appendProjectedPolyline(
            lineSegments,
            *renderData->preparedProjection,
            kGridAltitudeSampleCount,
            maxSegmentLengthSquared,
            renderData->renderTheme.gridAltitudeLine,
            kGridLineWidthPx,
            [altitudeDeg](const int index) {
                const double azimuthDeg = (360.0 * static_cast<double>(index))
                                          / static_cast<double>(kGridAltitudeSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = static_cast<double>(altitudeDeg),
                    .azimuthDeg = azimuthDeg
                };
            }
        );
    }

    for (int azimuthDeg = 0; azimuthDeg < 360; azimuthDeg += kGridAzimuthStepDeg) {
        if (azimuthDeg % 90 == 0) {
            continue;
        }

        appendProjectedPolyline(
            lineSegments,
            *renderData->preparedProjection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            renderData->renderTheme.gridAzimuthLine,
            kGridLineWidthPx,
            [azimuthDeg](const int index) {
                const double altitudeDeg = -85.0 + (170.0 * static_cast<double>(index))
                                                       / static_cast<double>(kGridAzimuthSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = altitudeDeg,
                    .azimuthDeg = static_cast<double>(azimuthDeg)
                };
            }
        );
    }

    constexpr std::array<int, 4> kCardinalAzimuths {0, 90, 180, 270};
    for (const int cardinalAzimuthDeg : kCardinalAzimuths) {
        appendProjectedPolyline(
            lineSegments,
            *renderData->preparedProjection,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            cardinalMeridianColor(cardinalAzimuthDeg, renderData->renderTheme),
            kCardinalLineWidthPx,
            [cardinalAzimuthDeg](const int index) {
                const double altitudeDeg = -85.0 + (170.0 * static_cast<double>(index))
                                                       / static_cast<double>(kGridAzimuthSampleCount);
                return skygate::core::HorizontalCoordinate {
                    .altitudeDeg = altitudeDeg,
                    .azimuthDeg = static_cast<double>(cardinalAzimuthDeg)
                };
            }
        );
    }

    appendProjectedPolyline(
        lineSegments,
        *renderData->preparedProjection,
        kHorizonSampleCount,
        maxSegmentLengthSquared,
        renderData->renderTheme.horizonLine,
        kHorizonLineWidthPx,
        [](const int index) {
            const double azimuthDeg = (360.0 * static_cast<double>(index))
                                      / static_cast<double>(kHorizonSampleCount);
            return skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = azimuthDeg};
        }
    );

    for (const auto& line : renderData->lines) {
        appendLineSegment(
            lineSegments,
            static_cast<float>(line.x1),
            static_cast<float>(line.y1),
            static_cast<float>(line.x2),
            static_cast<float>(line.y2),
            static_cast<float>(line.widthPx),
            line.color
        );
    }

    syncBatchedLineNodes(rootNode->lineRoot(), lineSegments);
    syncBatchedPointNodes(rootNode->pointRoot(), renderData->points);

    return rootNode;
}

void SkyViewportItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    synchronizeRenderData();
}

void SkyViewportItem::disconnectFromSceneModel()
{
    if (m_sceneFrameChangedConnection) {
        disconnect(m_sceneFrameChangedConnection);
    }

    m_sceneFrameChangedConnection = {};
}

void SkyViewportItem::synchronizeRenderData()
{
    auto nextRenderData = std::make_shared<ViewportRenderData>();
    nextRenderData->viewportWidth = width();
    nextRenderData->viewportHeight = height();

    if (
        m_skySceneModel != nullptr
        && nextRenderData->viewportWidth > 0.0
        && nextRenderData->viewportHeight > 0.0
    ) {
        m_skySceneModel->setViewportSize(
            nextRenderData->viewportWidth,
            nextRenderData->viewportHeight
        );
        nextRenderData->preparedProjection = m_skySceneModel->preparedProjection();
        if (auto* controller = qobject_cast<SkyContextController*>(
                m_skySceneModel->skyContextController()
            )) {
            nextRenderData->renderTheme = controller->renderTheme();
        }
        if (nextRenderData->preparedProjection.has_value()) {
            const auto lines = m_skySceneModel->renderLineSpan();
            nextRenderData->lines.assign(lines.begin(), lines.end());

            const auto points = m_skySceneModel->renderPointSpan();
            nextRenderData->points.assign(points.begin(), points.end());
        }
    }

    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        m_renderData = std::move(nextRenderData);
    }

    update();
}
