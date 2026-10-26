#pragma once

#include "scene/SkyViewportSnapshot.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>

#include <memory>
#include <mutex>

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
    void disconnectFromSceneModel();
    void synchronizeRenderData();

private:
    QPointer<SkySceneModel> m_skySceneModel;
    QMetaObject::Connection m_viewportSnapshotChangedConnection;
    std::mutex m_renderDataMutex;
    std::shared_ptr<const SkyViewportSnapshot> m_renderData;
};
