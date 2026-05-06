#pragma once

#include "SkyContextController.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportItem.hpp"

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSGGeometryNode>
#include <QSGNode>

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
