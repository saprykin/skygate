#pragma once

#include <skygate/testsupport/MessageLogDispatcher.hpp>

#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QtGlobal>

namespace skygate::ui::tests {

class QmlWarningScope final {
public:
    enum class Forwarding {
        Enabled,
        Disabled
    };

    explicit QmlWarningScope(const Forwarding forwarding = Forwarding::Enabled) : m_forwarding(forwarding)
    {
        skygate::testsupport::detail::MessageLogDispatcher::subscribe(this, &QmlWarningScope::messageHandler);
    }

    QmlWarningScope(const QmlWarningScope&) = delete;
    QmlWarningScope& operator=(const QmlWarningScope&) = delete;
    QmlWarningScope(QmlWarningScope&&) = delete;
    QmlWarningScope& operator=(QmlWarningScope&&) = delete;

    ~QmlWarningScope()
    {
        skygate::testsupport::detail::MessageLogDispatcher::unsubscribe(this, &QmlWarningScope::messageHandler);
    }

    [[nodiscard]] QStringList messages() const
    {
        QMutexLocker lock(&m_mutex);
        return m_messages;
    }

private:
    static skygate::testsupport::detail::MessageLogDispatcher::Result
    messageHandler(void* owner, const QtMsgType type, const QMessageLogContext& /*context*/, const QString& message)
    {
        QmlWarningScope* scope = static_cast<QmlWarningScope*>(owner);

        QMutexLocker lock(&scope->m_mutex);
        if (type == QtWarningMsg) {
            scope->m_messages.push_back(message);
        }

        if (scope->m_forwarding == Forwarding::Disabled) {
            return skygate::testsupport::detail::MessageLogDispatcher::Result::Stop;
        }
        return skygate::testsupport::detail::MessageLogDispatcher::Result::Continue;
    }

private:
    Forwarding m_forwarding = Forwarding::Enabled;
    QStringList m_messages;
    mutable QMutex m_mutex;
};

}  // namespace skygate::ui::tests
