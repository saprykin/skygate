#include "SkyViewportItem.hpp"
#include "SkyRenderGlyph.hpp"
#include "SkyRenderLine.hpp"
#include "SkyRenderPoint.hpp"
#include "SkyViewportGeometry.hpp"
#include "SkyViewportGeometryInput.hpp"
#include "SkyViewportLineSegment.hpp"
#include "SkyViewportSceneGraph.hpp"
#include "SkyViewportSceneRootNode.hpp"
#include "scene/SkySceneModel.hpp"

#include <memory>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

SkyViewportItem::SkyViewportItem(QQuickItem* parent) : QQuickItem(parent)
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
        m_viewportSnapshotChangedConnection = connect(
            m_skySceneModel, &SkySceneModel::viewportSnapshotChanged, this, &SkyViewportItem::synchronizeRenderData
        );
        m_skySceneModel->setViewportSize(width(), height());
    }

    emit skySceneModelChanged();
    synchronizeRenderData();
}

QSGNode* SkyViewportItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData)
{
    (void)updatePaintNodeData;

    auto* rootNode = static_cast<skygate::ui::internal::SkyViewportSceneRootNode*>(oldNode);
    if (rootNode == nullptr) {
        rootNode = new skygate::ui::internal::SkyViewportSceneRootNode();
    }

    std::shared_ptr<const SkyViewportSnapshot> renderData;
    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        renderData = m_renderData;
    }

    if (renderData == nullptr || !renderData->preparedProjection.has_value()) {
        skygate::ui::internal::SkyViewportSceneGraph::clearChildNodes(rootNode->lineRoot());
        skygate::ui::internal::SkyViewportSceneGraph::clearChildNodes(rootNode->pointRoot());
        return rootNode;
    }

    const std::vector<skygate::ui::internal::SkyViewportLineSegment> lineSegments =
        skygate::ui::internal::SkyViewportGeometry::buildLineSegments(
            skygate::ui::internal::SkyViewportGeometryInput{
                .projection = *renderData->preparedProjection,
                .viewportWidth = renderData->viewportWidth,
                .viewportHeight = renderData->viewportHeight,
                .renderTheme = renderData->renderTheme,
                .overlayLayers = renderData->overlayLayers,
                .observer = renderData->observationContext.observer,
                .utcTime = renderData->observationContext.utcTime,
                .renderLines = std::span<const SkyRenderLine>(renderData->lines.data(), renderData->lines.size()),
                .renderGlyphs = std::span<const SkyRenderGlyph>(renderData->glyphs.data(), renderData->glyphs.size())
            }
        );

    skygate::ui::internal::SkyViewportSceneGraph::clearChildNodes(rootNode->lineRoot());
    skygate::ui::internal::SkyViewportSceneGraph::clearChildNodes(rootNode->pointRoot());
    skygate::ui::internal::SkyViewportSceneGraph::syncLineNodes(rootNode->lineRoot(), lineSegments);
    skygate::ui::internal::SkyViewportSceneGraph::syncPointNodes(
        rootNode->pointRoot(), std::span<const SkyRenderPoint>(renderData->points.data(), renderData->points.size())
    );

    return rootNode;
}

void SkyViewportItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    synchronizeRenderData();
}

void SkyViewportItem::disconnectFromSceneModel()
{
    if (m_viewportSnapshotChangedConnection) {
        disconnect(m_viewportSnapshotChangedConnection);
    }

    m_viewportSnapshotChangedConnection = {};
}

void SkyViewportItem::synchronizeRenderData()
{
    std::shared_ptr<const SkyViewportSnapshot> nextRenderData;
    if (m_skySceneModel != nullptr && width() > 0.0 && height() > 0.0) {
        m_skySceneModel->setViewportSize(width(), height());
        nextRenderData = m_skySceneModel->viewportSnapshot();
    }

    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        m_renderData = std::move(nextRenderData);
    }

    update();
}
