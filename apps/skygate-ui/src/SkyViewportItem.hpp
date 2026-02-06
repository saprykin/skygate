#pragma once

#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>

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
    void disconnectFromContextController();

private:
    QPointer<SkyContextController> m_skyContextController;
    QMetaObject::Connection m_skyContextChangedConnection;
    QMetaObject::Connection m_projectionTypeChangedConnection;
};
