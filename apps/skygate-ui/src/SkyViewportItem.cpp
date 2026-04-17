#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {
constexpr int kHorizonSampleCount = 96;
constexpr int kGridAltitudeStepDeg = 15;
constexpr int kGridAzimuthStepDeg = 30;
constexpr int kGridAltitudeSampleCount = 96;
constexpr int kGridAzimuthSampleCount = 64;
const QColor kHorizonLineColor(255, 170, 92, 220);
const QColor kCardinalNorthLineColor(130, 216, 255, 132);
const QColor kCardinalEastLineColor(255, 208, 136, 132);
const QColor kCardinalSouthLineColor(255, 158, 158, 132);
const QColor kCardinalWestLineColor(156, 232, 198, 132);

struct LineSegment final {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
    QColor color;
};

struct VertexBucket final {
    QColor color;
    std::vector<float> coordinates;
};

[[nodiscard]] QColor cardinalMeridianColor(const int azimuthDeg)
{
    switch (azimuthDeg) {
    case 0:
        return kCardinalNorthLineColor;
    case 90:
        return kCardinalEastLineColor;
    case 180:
        return kCardinalSouthLineColor;
    case 270:
        return kCardinalWestLineColor;
    default:
        return QColor(140, 186, 236, 74);
    }
}

void appendLineSegment(
    std::vector<LineSegment>& lineSegments,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const QColor& color
)
{
    lineSegments.push_back(LineSegment {
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .color = color
    });
}

// Batch scene graph work by color so downloaded HYG catalogs do not create
// one node and one draw call per star.
void appendBatchedLineNodes(
    QSGNode* rootNode,
    const std::vector<LineSegment>& lineSegments
)
{
    if (rootNode == nullptr || lineSegments.empty()) {
        return;
    }

    std::unordered_map<std::uint32_t, VertexBucket> buckets;
    buckets.reserve(lineSegments.size());

    for (const auto& lineSegment : lineSegments) {
        const std::uint32_t colorKey = static_cast<std::uint32_t>(lineSegment.color.rgba());
        auto [bucketIt, inserted] = buckets.try_emplace(colorKey);
        if (inserted) {
            bucketIt->second.color = lineSegment.color;
            bucketIt->second.coordinates.reserve(32U);
        }

        auto& coordinates = bucketIt->second.coordinates;
        coordinates.push_back(lineSegment.x1);
        coordinates.push_back(lineSegment.y1);
        coordinates.push_back(lineSegment.x2);
        coordinates.push_back(lineSegment.y2);
    }

    for (auto& [colorKey, bucket] : buckets) {
        (void) colorKey;

        const int vertexCount = static_cast<int>(bucket.coordinates.size() / 2U);
        if (vertexCount <= 0) {
            continue;
        }

        QSGGeometryNode* node = new QSGGeometryNode();
        QSGGeometry* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), vertexCount);
        geometry->setDrawingMode(QSGGeometry::DrawLines);

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

void appendBatchedPointNodes(
    QSGNode* rootNode,
    const std::vector<SkyContextController::SkyRenderPoint>& points
)
{
    if (rootNode == nullptr || points.empty()) {
        return;
    }

    std::unordered_map<std::uint32_t, VertexBucket> buckets;
    buckets.reserve(points.size());

    for (const auto& point : points) {
        const std::uint32_t colorKey = static_cast<std::uint32_t>(point.color.rgba());
        auto [bucketIt, inserted] = buckets.try_emplace(colorKey);
        if (inserted) {
            bucketIt->second.color = point.color;
            bucketIt->second.coordinates.reserve(96U);
        }

        const float halfSize = static_cast<float>(point.sizePx * 0.5);
        const float left = static_cast<float>(point.x) - halfSize;
        const float right = static_cast<float>(point.x) + halfSize;
        const float top = static_cast<float>(point.y) - halfSize;
        const float bottom = static_cast<float>(point.y) + halfSize;

        auto& coordinates = bucketIt->second.coordinates;
        coordinates.push_back(left);
        coordinates.push_back(top);
        coordinates.push_back(right);
        coordinates.push_back(top);
        coordinates.push_back(left);
        coordinates.push_back(bottom);

        coordinates.push_back(left);
        coordinates.push_back(bottom);
        coordinates.push_back(right);
        coordinates.push_back(top);
        coordinates.push_back(right);
        coordinates.push_back(bottom);
    }

    for (auto& [colorKey, bucket] : buckets) {
        (void) colorKey;

        const int vertexCount = static_cast<int>(bucket.coordinates.size() / 2U);
        if (vertexCount <= 0) {
            continue;
        }

        QSGGeometryNode* node = new QSGGeometryNode();
        QSGGeometry* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), vertexCount);
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
    const SkyContextController* skyContextController,
    const double viewportWidth,
    const double viewportHeight,
    const int sampleCount,
    const double maxSegmentLengthSquared,
    const QColor& color,
    CoordinateFn coordinateFn
)
{
    if (skyContextController == nullptr || sampleCount <= 0) {
        return;
    }

    bool hasPreviousPoint = false;
    auto previousPoint = skyContextController->projectHorizontal(
        coordinateFn(0),
        viewportWidth,
        viewportHeight
    );

    for (int index = 0; index <= sampleCount; ++index) {
        const auto projected = skyContextController->projectHorizontal(
            coordinateFn(index),
            viewportWidth,
            viewportHeight
        );

        if (!projected.isVisible) {
            hasPreviousPoint = false;
            continue;
        }

        if (hasPreviousPoint) {
            const double deltaX = projected.x - previousPoint.x;
            const double deltaY = projected.y - previousPoint.y;
            const double segmentLengthSquared = deltaX * deltaX + deltaY * deltaY;
            if (segmentLengthSquared <= maxSegmentLengthSquared) {
                appendLineSegment(
                    lineSegments,
                    static_cast<float>(previousPoint.x),
                    static_cast<float>(previousPoint.y),
                    static_cast<float>(projected.x),
                    static_cast<float>(projected.y),
                    color
                );
            }
        }

        previousPoint = projected;
        hasPreviousPoint = true;
    }
}

void clearChildNodes(QSGNode* rootNode)
{
    if (rootNode == nullptr) {
        return;
    }

    QSGNode* child = rootNode->firstChild();
    while (child != nullptr) {
        QSGNode* next = child->nextSibling();
        rootNode->removeChildNode(child);
        delete child;
        child = next;
    }
}

}  // namespace

SkyViewportItem::SkyViewportItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

QObject* SkyViewportItem::skyContextController() const noexcept
{
    return m_skyContextController;
}

void SkyViewportItem::setSkyContextController(QObject* skyContextController)
{
    SkyContextController* controller = qobject_cast<SkyContextController*>(skyContextController);
    if (m_skyContextController == controller) {
        return;
    }

    disconnectFromContextController();
    m_skyContextController = controller;

    if (m_skyContextController != nullptr) {
        m_skyContextChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::skyContextChanged,
            this,
            &SkyViewportItem::update
        );

        m_projectionTypeChangedConnection = connect(
            m_skyContextController,
            &SkyContextController::projectionTypeChanged,
            this,
            &SkyViewportItem::update
        );
    }

    emit skyContextControllerChanged();
    update();
}

QSGNode* SkyViewportItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData)
{
    (void)updatePaintNodeData;

    QSGNode* rootNode = oldNode;
    if (rootNode == nullptr) {
        rootNode = new QSGNode();
    }

    clearChildNodes(rootNode);

    if (m_skyContextController == nullptr) {
        return rootNode;
    }

    const double viewportWidth = width();
    const double viewportHeight = height();
    const double maxSegmentLength = std::max(viewportWidth, viewportHeight) * 0.30;
    const double maxSegmentLengthSquared = maxSegmentLength * maxSegmentLength;
    std::vector<LineSegment> lineSegments;
    lineSegments.reserve(4096U);

    for (int altitudeDeg = -75; altitudeDeg <= 75; altitudeDeg += kGridAltitudeStepDeg) {
        if (altitudeDeg == 0) {
            continue;
        }

        appendProjectedPolyline(
            lineSegments,
            m_skyContextController,
            viewportWidth,
            viewportHeight,
            kGridAltitudeSampleCount,
            maxSegmentLengthSquared,
            QColor(112, 146, 194, 70),
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
            m_skyContextController,
            viewportWidth,
            viewportHeight,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            QColor(102, 138, 184, 58),
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
            m_skyContextController,
            viewportWidth,
            viewportHeight,
            kGridAzimuthSampleCount,
            maxSegmentLengthSquared,
            cardinalMeridianColor(cardinalAzimuthDeg),
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
        m_skyContextController,
        viewportWidth,
        viewportHeight,
        kHorizonSampleCount,
        maxSegmentLengthSquared,
        kHorizonLineColor,
        [](const int index) {
            const double azimuthDeg = (360.0 * static_cast<double>(index))
                                      / static_cast<double>(kHorizonSampleCount);
            return skygate::core::HorizontalCoordinate {.altitudeDeg = 0.0, .azimuthDeg = azimuthDeg};
        }
    );

    const auto lines = m_skyContextController->renderConstellationLines(viewportWidth, viewportHeight);
    for (const auto& line : lines) {
        appendLineSegment(
            lineSegments,
            static_cast<float>(line.x1),
            static_cast<float>(line.y1),
            static_cast<float>(line.x2),
            static_cast<float>(line.y2),
            line.color
        );
    }
    appendBatchedLineNodes(rootNode, lineSegments);

    const auto points = m_skyContextController->renderPoints(viewportWidth, viewportHeight);
    appendBatchedPointNodes(rootNode, points);

    return rootNode;
}

void SkyViewportItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

void SkyViewportItem::disconnectFromContextController()
{
    if (m_skyContextChangedConnection) {
        disconnect(m_skyContextChangedConnection);
    }
    if (m_projectionTypeChangedConnection) {
        disconnect(m_projectionTypeChangedConnection);
    }

    m_skyContextChangedConnection = {};
    m_projectionTypeChangedConnection = {};
}
