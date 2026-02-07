#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

#include <algorithm>
#include <array>

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

template <typename CoordinateFn>
void appendProjectedPolyline(
    QSGNode* rootNode,
    const SkyContextController* skyContextController,
    const double viewportWidth,
    const double viewportHeight,
    const int sampleCount,
    const double maxSegmentLengthSquared,
    const QColor& color,
    CoordinateFn coordinateFn
)
{
    if (rootNode == nullptr || skyContextController == nullptr || sampleCount <= 0) {
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
                appendLineSegmentNode(
                    rootNode,
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

    for (int altitudeDeg = -75; altitudeDeg <= 75; altitudeDeg += kGridAltitudeStepDeg) {
        if (altitudeDeg == 0) {
            continue;
        }

        appendProjectedPolyline(
            rootNode,
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
            rootNode,
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
            rootNode,
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
        rootNode,
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
        appendLineSegmentNode(
            rootNode,
            static_cast<float>(line.x1),
            static_cast<float>(line.y1),
            static_cast<float>(line.x2),
            static_cast<float>(line.y2),
            line.color
        );
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
