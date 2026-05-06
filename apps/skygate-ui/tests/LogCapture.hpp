#pragma once

#include <QList>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace skygate::ui::tests {

class LogCapture final {
public:
    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg)
        : m_minimumType(minimumType)
    {
        QMutexLocker lock(&s_mutex);
        if (s_captures.isEmpty()) {
            s_previousHandler = qInstallMessageHandler(&LogCapture::handler);
        }
        s_captures.push_back(this);
    }

    ~LogCapture()
    {
        QMutexLocker lock(&s_mutex);
        s_captures.removeAll(this);
        if (s_captures.isEmpty()) {
            const QtMessageHandler previousHandler = s_previousHandler;
            s_previousHandler = nullptr;
            lock.unlock();
            qInstallMessageHandler(previousHandler);
        }
    }

    [[nodiscard]] QString joinedMessages() const
    {
        QMutexLocker lock(&s_mutex);
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
        QMutexLocker lock(&s_mutex);
        for (LogCapture* capture : s_captures) {
            if (severityRank(type) >= severityRank(capture->m_minimumType)) {
                capture->m_messages.push_back(QStringLiteral("%1 %2").arg(
                    QString::fromUtf8(context.category != nullptr ? context.category : "default"),
                    message
                ));
            }
        }
    }

private:
    QtMsgType m_minimumType = QtWarningMsg;
    QStringList m_messages;
    static inline QMutex s_mutex;
    static inline QList<LogCapture*> s_captures;
    static inline QtMessageHandler s_previousHandler = nullptr;
};

}  // namespace skygate::ui::tests
