#pragma once

#include "SkyRenderPoint.hpp"
#include "SkyViewportLineSegment.hpp"

#include <QSGNode>

#include <span>

namespace skygate::ui::internal {

class SkyViewportSceneGraph final {
public:
    static void clearChildNodes(QSGNode* rootNode);

    static void syncLineNodes(QSGNode* rootNode, std::span<const SkyViewportLineSegment> lineSegments);

    static void syncPointNodes(QSGNode* rootNode, std::span<const SkyRenderPoint> points);
};

}  // namespace skygate::ui::internal
