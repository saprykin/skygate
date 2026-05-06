#pragma once

#include "SkyRenderFrame.hpp"
#include "SkyViewportGeometry.hpp"

#include <QSGNode>

#include <span>

namespace skygate::ui::internal {

class SkyViewportSceneRootNode final : public QSGNode {
public:
    SkyViewportSceneRootNode();

    [[nodiscard]] QSGNode* lineRoot() const noexcept;
    [[nodiscard]] QSGNode* pointRoot() const noexcept;

private:
    QSGNode* m_lineRoot = nullptr;
    QSGNode* m_pointRoot = nullptr;
};

void clearSkyViewportChildNodes(QSGNode* rootNode);

void syncSkyViewportLineNodes(
    QSGNode* rootNode,
    std::span<const SkyViewportLineSegment> lineSegments
);

void syncSkyViewportPointNodes(
    QSGNode* rootNode,
    std::span<const SkyRenderPoint> points
);

}  // namespace skygate::ui::internal
