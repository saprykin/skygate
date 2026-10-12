#include "SkyViewportSceneRootNode.hpp"

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

}  // namespace skygate::ui::internal
