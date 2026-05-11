#pragma once

#include "SkyRenderBuilders.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>

#include "skygate/core/PreparedProjection.hpp"

#include <memory>
#include <mutex>
#include <optional>

class SkySceneModel;
class QSGNode;

class SkyViewportItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* skySceneModel READ skySceneModel WRITE setSkySceneModel NOTIFY skySceneModelChanged)

public:
    explicit SkyViewportItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QObject* skySceneModel() const noexcept;
    void setSkySceneModel(QObject* skySceneModel);

signals:
    void skySceneModelChanged();

protected:
    [[nodiscard]] QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct ViewportRenderData;

    void disconnectFromSceneModel();
    void synchronizeRenderData();

private:
    QPointer<SkySceneModel> m_skySceneModel;
    QMetaObject::Connection m_sceneFrameChangedConnection;
    std::mutex m_renderDataMutex;
    std::shared_ptr<const ViewportRenderData> m_renderData;
};
