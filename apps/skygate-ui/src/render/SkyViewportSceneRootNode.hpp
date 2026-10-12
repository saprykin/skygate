#pragma once

#include <QSGNode>

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

}  // namespace skygate::ui::internal
