#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>

namespace {
constexpr int kHorizonSampleCount = 96;

void appendLineSegmentNode(
    QSGNode* rootNode,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const QColor& color
)
{
    QSGGeometryNode* node = new QSGGeometryNode();
    QSGGeometry* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
    geometry->setDrawingMode(QSGGeometry::DrawLines);

    auto* vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(x1, y1);
    vertices[1].set(x2, y2);

    QSGFlatColorMaterial* material = new QSGFlatColorMaterial();
    material->setColor(color);

    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry, true);
    node->setFlag(QSGNode::OwnsMaterial, true);
    rootNode->appendChildNode(node);
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
    bool hasPreviousHorizonPoint = false;
    auto previousHorizonPoint = m_skyContextController->projectHorizontal(
        {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        viewportWidth,
        viewportHeight
    );

    for (int index = 0; index <= kHorizonSampleCount; ++index) {
        const double azimuthDeg = (360.0 * static_cast<double>(index)) / static_cast<double>(kHorizonSampleCount);
        const auto projected = m_skyContextController->projectHorizontal(
            {.altitudeDeg = 0.0, .azimuthDeg = azimuthDeg},
            viewportWidth,
            viewportHeight
        );

        if (!projected.isVisible) {
            hasPreviousHorizonPoint = false;
            continue;
        }

        if (hasPreviousHorizonPoint) {
            const double deltaX = projected.x - previousHorizonPoint.x;
            const double deltaY = projected.y - previousHorizonPoint.y;
            const double segmentLengthSquared = deltaX * deltaX + deltaY * deltaY;
            if (segmentLengthSquared <= maxSegmentLengthSquared) {
                appendLineSegmentNode(
                    rootNode,
                    static_cast<float>(previousHorizonPoint.x),
                    static_cast<float>(previousHorizonPoint.y),
                    static_cast<float>(projected.x),
                    static_cast<float>(projected.y),
                    QColor(110, 156, 216, 170)
                );
            }
        }

        previousHorizonPoint = projected;
        hasPreviousHorizonPoint = true;
    }

    const auto points = m_skyContextController->renderPoints(viewportWidth, viewportHeight);

    for (const auto& point : points) {
        QSGGeometryNode* node = new QSGGeometryNode();
        QSGGeometry* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);

        const float halfSize = static_cast<float>(point.sizePx * 0.5);
        const float x = static_cast<float>(point.x);
        const float y = static_cast<float>(point.y);

        auto* vertices = geometry->vertexDataAsPoint2D();
        vertices[0].set(x - halfSize, y - halfSize);
        vertices[1].set(x + halfSize, y - halfSize);
        vertices[2].set(x - halfSize, y + halfSize);
        vertices[3].set(x + halfSize, y + halfSize);

        QSGFlatColorMaterial* material = new QSGFlatColorMaterial();
        material->setColor(point.color);

        node->setGeometry(geometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsGeometry, true);
        node->setFlag(QSGNode::OwnsMaterial, true);

        rootNode->appendChildNode(node);
    }

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
