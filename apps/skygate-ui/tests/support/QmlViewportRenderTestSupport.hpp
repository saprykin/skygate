#pragma once

#include "SkyContextController.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QColor>
#include <QRectF>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QStringList>

#include <cstddef>
#include <memory>

namespace skygate::ui::tests {

class TestSkyViewportItem final : public SkyViewportItem {
public:
    [[nodiscard]] QSGNode* buildPaintNode()
    {
        return updatePaintNode(nullptr, nullptr);
    }
};

inline int nodeTreeSize(const QSGNode* node)
{
    if (node == nullptr) {
        return 0;
    }

    int count = 1;
    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        count += nodeTreeSize(child);
    }
    return count;
}

struct SkyViewRenderSignature final {
    int sceneGraphNodeCount = 0;
    int sceneGraphVertexCount = 0;
    std::size_t renderLineCount = 0;
    std::size_t renderPointCount = 0;
    std::size_t renderGlyphCount = 0;
    int overlayItemCount = 0;
    SkyOverlayLayerVisibility overlayLayers;

    [[nodiscard]] bool differsFrom(const SkyViewRenderSignature& other) const noexcept
    {
        return sceneGraphNodeCount != other.sceneGraphNodeCount
            || sceneGraphVertexCount != other.sceneGraphVertexCount
            || renderLineCount != other.renderLineCount
            || renderPointCount != other.renderPointCount
            || renderGlyphCount != other.renderGlyphCount
            || overlayItemCount != other.overlayItemCount
            || !overlayLayers.equals(other.overlayLayers);
    }
};

inline int geometryVertexCount(const QSGNode* node)
{
    if (node == nullptr) {
        return 0;
    }

    int vertexCount = 0;
    if (node->type() == QSGNode::GeometryNodeType) {
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(node);
        if (geometryNode->geometry() != nullptr) {
            vertexCount += geometryNode->geometry()->vertexCount();
        }
    }

    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        vertexCount += geometryVertexCount(child);
    }
    return vertexCount;
}

struct SkyViewGeometryProbe final {
    int vertexCount = 0;
    QRectF bounds;
    bool hasBounds = false;

    [[nodiscard]] bool hasGeometry() const noexcept
    {
        return vertexCount > 0 && hasBounds;
    }
};

inline void expandProbeBounds(SkyViewGeometryProbe& probe, const QPointF& point)
{
    if (!probe.hasBounds) {
        probe.bounds = QRectF(point, QSizeF(0.0, 0.0));
        probe.hasBounds = true;
        return;
    }
    probe.bounds = probe.bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
}

inline SkyViewGeometryProbe geometryProbe(const QSGNode* node)
{
    SkyViewGeometryProbe probe;
    if (node == nullptr) {
        return probe;
    }

    if (node->type() == QSGNode::GeometryNodeType) {
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(node);
        const QSGGeometry* geometry = geometryNode->geometry();
        if (geometry != nullptr) {
            const int vertexCount = geometry->vertexCount();
            const auto* vertices = geometry->vertexDataAsPoint2D();
            for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                expandProbeBounds(
                    probe,
                    QPointF(vertices[vertexIndex].x, vertices[vertexIndex].y)
                );
            }
            probe.vertexCount += vertexCount;
        }
    }

    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        const SkyViewGeometryProbe childProbe = geometryProbe(child);
        probe.vertexCount += childProbe.vertexCount;
        if (childProbe.hasBounds) {
            expandProbeBounds(probe, childProbe.bounds.topLeft());
            expandProbeBounds(probe, childProbe.bounds.bottomRight());
        }
    }
    return probe;
}

inline bool geometryProbeIntersectsViewport(
    const SkyViewGeometryProbe& probe,
    const QSizeF& viewportSize,
    const double tolerancePx = 2.0
)
{
    if (!probe.hasGeometry()) {
        return false;
    }

    const QRectF relaxedViewport(
        -tolerancePx,
        -tolerancePx,
        viewportSize.width() + (tolerancePx * 2.0),
        viewportSize.height() + (tolerancePx * 2.0)
    );
    return relaxedViewport.intersects(probe.bounds)
        || relaxedViewport.contains(probe.bounds.center());
}

inline SkyViewGeometryProbe geometryMaterialColorProbe(const QSGNode* node, const QColor& color)
{
    SkyViewGeometryProbe probe;
    if (node == nullptr) {
        return probe;
    }

    if (node->type() == QSGNode::GeometryNodeType) {
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(node);
        const QSGGeometry* geometry = geometryNode->geometry();
        if (geometry != nullptr && geometryNode->material() != nullptr) {
            const auto* material = dynamic_cast<const QSGFlatColorMaterial*>(geometryNode->material());
            if (material != nullptr && material->color().rgba() == color.rgba()) {
                const int vertexCount = geometry->vertexCount();
                const auto* vertices = geometry->vertexDataAsPoint2D();
                for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                    expandProbeBounds(
                        probe,
                        QPointF(vertices[vertexIndex].x, vertices[vertexIndex].y)
                    );
                }
                probe.vertexCount += vertexCount;
            }
        }
    }

    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        const SkyViewGeometryProbe childProbe = geometryMaterialColorProbe(child, color);
        probe.vertexCount += childProbe.vertexCount;
        if (childProbe.hasBounds) {
            expandProbeBounds(probe, childProbe.bounds.topLeft());
            expandProbeBounds(probe, childProbe.bounds.bottomRight());
        }
    }
    return probe;
}

inline void collectGeometryMaterialColorNames(const QSGNode* node, QStringList& colorNames)
{
    if (node == nullptr) {
        return;
    }

    if (node->type() == QSGNode::GeometryNodeType) {
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(node);
        if (geometryNode->material() != nullptr) {
            const auto* material = dynamic_cast<const QSGFlatColorMaterial*>(geometryNode->material());
            const QString colorName = material != nullptr
                ? material->color().name(QColor::HexArgb)
                : QStringLiteral("<non-flat>");
            if (!colorNames.contains(colorName)) {
                colorNames.push_back(colorName);
            }
        }
    }

    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling()) {
        collectGeometryMaterialColorNames(child, colorNames);
    }
}

inline QStringList geometryMaterialColorNames(const QSGNode* node)
{
    QStringList colorNames;
    collectGeometryMaterialColorNames(node, colorNames);
    colorNames.sort();
    return colorNames;
}

inline SkyViewRenderSignature renderSignature(
    SkyContextController& controller,
    SkySceneModel& sceneModel
)
{
    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(&sceneModel);
    for (int attempt = 0; attempt < 100 && sceneModel.snapshotGeneration() == 0U; ++attempt) {
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    return SkyViewRenderSignature {
        .sceneGraphNodeCount = nodeTreeSize(paintNode.get()),
        .sceneGraphVertexCount = geometryVertexCount(paintNode.get()),
        .renderLineCount = sceneModel.renderLineSpan().size(),
        .renderPointCount = sceneModel.renderPointSpan().size(),
        .renderGlyphCount = sceneModel.renderGlyphSpan().size(),
        .overlayItemCount = static_cast<int>(sceneModel.overlayItems().size()),
        .overlayLayers = controller.overlayLayerVisibility()
    };
}

}  // namespace skygate::ui::tests
