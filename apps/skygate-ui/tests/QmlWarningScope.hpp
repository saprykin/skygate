#pragma once

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
        s_currentScope = this;
        m_previousHandler = qInstallMessageHandler(&QmlWarningScope::messageHandler);
    }

    ~QmlWarningScope()
    {
        qInstallMessageHandler(m_previousHandler);
        s_currentScope = nullptr;
    }

    [[nodiscard]] QStringList messages() const
    {
        return m_messages;
    }

private:
    static void messageHandler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (s_currentScope != nullptr && type == QtWarningMsg) {
            s_currentScope->m_messages.push_back(message);
        }

        if (
            s_currentScope != nullptr
            && s_currentScope->m_forwarding == Forwarding::Enabled
            && s_currentScope->m_previousHandler != nullptr
        ) {
            s_currentScope->m_previousHandler(type, context, message);
        }
    }

private:
    Forwarding m_forwarding = Forwarding::Enabled;
    QtMessageHandler m_previousHandler = nullptr;
    QStringList m_messages;
    static inline QmlWarningScope* s_currentScope = nullptr;
};

}  // namespace skygate::ui::tests
