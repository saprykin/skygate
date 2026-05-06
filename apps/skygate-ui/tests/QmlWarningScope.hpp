#pragma once

#include <QList>
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

    explicit QmlWarningScope(const Forwarding forwarding = Forwarding::Enabled) :
        m_forwarding(forwarding)
    {
        QMutexLocker lock(&s_mutex);
        if (s_scopes.isEmpty()) {
            s_previousHandler = qInstallMessageHandler(&QmlWarningScope::messageHandler);
        }
        s_scopes.push_back(this);
    }

    ~QmlWarningScope()
    {
        QMutexLocker lock(&s_mutex);
        s_scopes.removeAll(this);
        if (s_scopes.isEmpty()) {
            const QtMessageHandler previousHandler = s_previousHandler;
            s_previousHandler = nullptr;
            lock.unlock();
            qInstallMessageHandler(previousHandler);
        }
    }

    [[nodiscard]] QStringList messages() const
    {
        QMutexLocker lock(&s_mutex);
        return m_messages;
    }

private:
    static void messageHandler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        QtMessageHandler previousHandler = nullptr;
        bool forwardToPreviousHandler = true;

        {
            QMutexLocker lock(&s_mutex);
            for (qsizetype index = s_scopes.size() - 1; index >= 0; --index) {
                QmlWarningScope* scope = s_scopes.at(index);
                if (type == QtWarningMsg) {
                    scope->m_messages.push_back(message);
                }
                if (scope->m_forwarding == Forwarding::Disabled) {
                    forwardToPreviousHandler = false;
                    break;
                }
            }
            previousHandler = s_previousHandler;
        }

        if (forwardToPreviousHandler && previousHandler != nullptr) {
            previousHandler(type, context, message);
        }
    }

private:
    Forwarding m_forwarding = Forwarding::Enabled;
    QStringList m_messages;
    static inline QMutex s_mutex;
    static inline QList<QmlWarningScope*> s_scopes;
    static inline QtMessageHandler s_previousHandler = nullptr;
};

}  // namespace skygate::ui::tests
