#pragma once

#include "SkyRenderBuilders.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>

#include "skygate/core/PreparedProjection.hpp"

#include <memory>
#include <mutex>
#include <optional>

class SkyContextController;
class QSGNode;

class SkyViewportItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* skyContextController READ skyContextController WRITE setSkyContextController NOTIFY skyContextControllerChanged)

public:
    explicit SkyViewportItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QObject* skyContextController() const noexcept;
    void setSkyContextController(QObject* skyContextController);

signals:
    void skyContextControllerChanged();

protected:
    [[nodiscard]] QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct ViewportRenderData;

    void disconnectFromContextController();
    void synchronizeRenderData();

private:
    QPointer<SkyContextController> m_skyContextController;
    QMetaObject::Connection m_skyContextChangedConnection;
    QMetaObject::Connection m_projectionTypeChangedConnection;
    std::mutex m_renderDataMutex;
    std::shared_ptr<const ViewportRenderData> m_renderData;
};
