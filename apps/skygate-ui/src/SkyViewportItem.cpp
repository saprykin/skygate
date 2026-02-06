#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

namespace {

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

    const auto points = m_skyContextController->renderPoints(width(), height());

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
