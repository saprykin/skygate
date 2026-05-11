#include "SkyViewportSceneGraph.hpp"

#include "skygate/core/math/Geometry2d.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace {

struct VertexBucket final {
    QColor color;
    float widthPx = 1.0F;
    std::vector<float> coordinates;
};

void appendBucketNode(
    QSGNode* rootNode,
    const VertexBucket& bucket
)
{
    const int vertexCount = static_cast<int>(bucket.coordinates.size() / 2U);
    if (vertexCount <= 0) {
        return;
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

}  // namespace

namespace skygate::ui::internal {

SkyViewportSceneRootNode::SkyViewportSceneRootNode()
{
    m_lineRoot = new QSGNode();
    m_pointRoot = new QSGNode();
    appendChildNode(m_lineRoot);
    appendChildNode(m_pointRoot);
}

QSGNode* SkyViewportSceneRootNode::lineRoot() const noexcept
{
    return m_lineRoot;
}

QSGNode* SkyViewportSceneRootNode::pointRoot() const noexcept
{
    return m_pointRoot;
}

void clearSkyViewportChildNodes(QSGNode* rootNode)
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

void syncSkyViewportLineNodes(
    QSGNode* rootNode,
    const std::span<const SkyViewportLineSegment> lineSegments
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

        const std::optional<skygate::core::Vector2d> offset =
            skygate::core::perpendicularOffset2d(
                lineSegment.x1,
                lineSegment.y1,
                lineSegment.x2,
                lineSegment.y2,
                lineSegment.widthPx * 0.5F
            );
        if (!offset.has_value()) {
            continue;
        }

        const float offsetX = static_cast<float>(offset->x);
        const float offsetY = static_cast<float>(offset->y);

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
        appendBucketNode(rootNode, bucket);
    }
}

void syncSkyViewportPointNodes(
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
        appendBucketNode(rootNode, bucket);
    }
}

}  // namespace skygate::ui::internal
