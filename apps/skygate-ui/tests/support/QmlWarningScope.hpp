#pragma once

#include <skygate/testsupport/MessageLogDispatcher.hpp>

#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <utility>

namespace skygate::ui::tests {

class QmlWarningScope final {
public:
    enum class Forwarding {
        Enabled,
        Disabled
    };

    /// Warnings from these logging categories describe the application state a
    /// test happens to run against rather than a QML problem, so the scope does
    /// not collect them.
    using IgnoredCategories = QStringList;

    explicit QmlWarningScope(
        const Forwarding forwarding = Forwarding::Enabled, IgnoredCategories ignoredCategories = {}
    )
        : m_forwarding(forwarding), m_ignoredCategories(std::move(ignoredCategories))
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
    messageHandler(void* owner, const QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        QmlWarningScope* scope = static_cast<QmlWarningScope*>(owner);

        QMutexLocker lock(&scope->m_mutex);
        if (type == QtWarningMsg && !scope->ignores(context)) {
            scope->m_messages.push_back(message);
        }

        if (scope->m_forwarding == Forwarding::Disabled) {
            return skygate::testsupport::detail::MessageLogDispatcher::Result::Stop;
        }
        return skygate::testsupport::detail::MessageLogDispatcher::Result::Continue;
    }

    [[nodiscard]] bool ignores(const QMessageLogContext& context) const
    {
        if (context.category == nullptr || m_ignoredCategories.isEmpty()) {
            return false;
        }

        return m_ignoredCategories.contains(QString::fromUtf8(context.category));
    }

    Forwarding m_forwarding = Forwarding::Enabled;
    IgnoredCategories m_ignoredCategories;
    QStringList m_messages;
    mutable QMutex m_mutex;
};

}  // namespace skygate::ui::tests
