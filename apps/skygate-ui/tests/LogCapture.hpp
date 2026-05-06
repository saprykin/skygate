#pragma once

#include <QMessageLogContext>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace skygate::ui::tests {

class LogCapture final {
public:
    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg)
        : m_minimumType(minimumType)
    {
        s_current = this;
        m_previousHandler = qInstallMessageHandler(&LogCapture::handler);
    }

    ~LogCapture()
    {
        qInstallMessageHandler(m_previousHandler);
        s_current = nullptr;
    }

    [[nodiscard]] QString joinedMessages() const
    {
        return m_messages.join('\n');
    }

private:
    [[nodiscard]] static int severityRank(const QtMsgType type) noexcept
    {
        switch (type) {
        case QtDebugMsg:
            return 0;
        case QtInfoMsg:
            return 1;
        case QtWarningMsg:
            return 2;
        case QtCriticalMsg:
            return 3;
        case QtFatalMsg:
            return 4;
        }

        return 1;
    }

    static void handler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (
            s_current != nullptr
            && severityRank(type) >= severityRank(s_current->m_minimumType)
        ) {
            s_current->m_messages.push_back(QStringLiteral("%1 %2").arg(
                QString::fromUtf8(context.category != nullptr ? context.category : "default"),
                message
            ));
        }
    }

private:
    QtMessageHandler m_previousHandler = nullptr;
    QtMsgType m_minimumType = QtWarningMsg;
    QStringList m_messages;
    static inline LogCapture* s_current = nullptr;
};

}  // namespace skygate::ui::tests
