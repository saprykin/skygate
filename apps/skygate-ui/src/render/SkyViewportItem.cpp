#include "SkyViewportItem.hpp"

#include "SkyContextController.hpp"
#include "SkySceneModel.hpp"
#include "SkyViewportGeometry.hpp"
#include "SkyViewportSceneGraph.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

struct SkyViewportItem::ViewportRenderData final {
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
    std::optional<skygate::core::PreparedProjection> preparedProjection;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    SkyOverlayLayerVisibility overlayLayers;
    skygate::core::GeoLocation observer;
    skygate::core::UtcTimePoint utcTime {};
    std::vector<SkyRenderLine> lines;
    std::vector<SkyRenderPoint> points;
    std::vector<SkyRenderGlyph> glyphs;
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

    auto* rootNode = static_cast<skygate::ui::internal::SkyViewportSceneRootNode*>(oldNode);
    if (rootNode == nullptr) {
        rootNode = new skygate::ui::internal::SkyViewportSceneRootNode();
    }

    std::shared_ptr<const ViewportRenderData> renderData;
    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        renderData = m_renderData;
    }

    if (renderData == nullptr || !renderData->preparedProjection.has_value()) {
        skygate::ui::internal::clearSkyViewportChildNodes(rootNode->lineRoot());
        skygate::ui::internal::clearSkyViewportChildNodes(rootNode->pointRoot());
        skygate::ui::internal::syncSkyViewportLineNodes(rootNode->lineRoot(), {});
        skygate::ui::internal::syncSkyViewportPointNodes(rootNode->pointRoot(), {});
        return rootNode;
    }

    const std::vector<skygate::ui::internal::SkyViewportLineSegment> lineSegments =
        skygate::ui::internal::buildSkyViewportLineSegments(
            skygate::ui::internal::SkyViewportGeometryInput {
                .projection = *renderData->preparedProjection,
                .viewportWidth = renderData->viewportWidth,
                .viewportHeight = renderData->viewportHeight,
                .renderTheme = renderData->renderTheme,
                .overlayLayers = renderData->overlayLayers,
                .observer = renderData->observer,
                .utcTime = renderData->utcTime,
                .renderLines = std::span<const SkyRenderLine>(
                    renderData->lines.data(),
                    renderData->lines.size()
                ),
                .renderGlyphs = std::span<const SkyRenderGlyph>(
                    renderData->glyphs.data(),
                    renderData->glyphs.size()
                )
            }
        );

    skygate::ui::internal::clearSkyViewportChildNodes(rootNode->lineRoot());
    skygate::ui::internal::clearSkyViewportChildNodes(rootNode->pointRoot());
    skygate::ui::internal::syncSkyViewportLineNodes(rootNode->lineRoot(), lineSegments);
    skygate::ui::internal::syncSkyViewportPointNodes(
        rootNode->pointRoot(),
        std::span<const SkyRenderPoint>(renderData->points.data(), renderData->points.size())
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
            nextRenderData->overlayLayers = controller->overlayLayerVisibility();
            nextRenderData->observer = controller->skyContext().observer;
            nextRenderData->utcTime = controller->skyContext().utcTime;
        }
        if (nextRenderData->preparedProjection.has_value()) {
            const auto lines = m_skySceneModel->renderLineSpan();
            nextRenderData->lines.assign(lines.begin(), lines.end());

            const auto points = m_skySceneModel->renderPointSpan();
            nextRenderData->points.assign(points.begin(), points.end());
            const auto glyphs = m_skySceneModel->renderGlyphSpan();
            nextRenderData->glyphs.assign(glyphs.begin(), glyphs.end());
        }
    }

    {
        std::scoped_lock<std::mutex> lock(m_renderDataMutex);
        m_renderData = std::move(nextRenderData);
    }

    update();
}
