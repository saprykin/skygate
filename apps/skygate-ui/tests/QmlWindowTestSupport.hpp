#pragma once

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QImage>
#include <QInputMethodEvent>
#include <QMetaObject>
#include <QPoint>
#include <QPointF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QString>
#include <QWheelEvent>
#include <QWindow>

namespace skygate::ui::tests {

class ExposedQuickWindow final {
public:
    explicit ExposedQuickWindow(QQuickItem* item)
    {
        m_window.resize(900, 640);
        item->setParentItem(m_window.contentItem());
        item->setWidth(m_window.width());
        item->setHeight(m_window.height());
        m_window.show();
        (void)QTest::qWaitForWindowExposed(&m_window);
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QQuickWindow* window() noexcept
    {
        return &m_window;
    }

private:
    QQuickWindow m_window;
};

inline void commitText(QWindow* window, const QString& text)
{
    QInputMethodEvent event;
    event.setCommitString(text);
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
}

inline void replaceText(QWindow* window, QQuickItem* item, const QString& text)
{
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "forceActiveFocus"));
    QVERIFY(QMetaObject::invokeMethod(item, "selectAll"));
    commitText(window, text);
}

}  // namespace skygate::ui::tests
