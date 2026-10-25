#pragma once

#include "skygate/testsupport/MessageLogDispatcher.hpp"

#include <QList>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <utility>

namespace skygate::testsupport {

class LogCapture final {
public:
    struct Options final {
        /// Messages below this severity are not recorded.
        QtMsgType minimumType = QtWarningMsg;
        /// Messages from these logging categories are not recorded.
        QStringList ignoredCategories;
        /// Record each message as "<category> <message>" instead of the bare text.
        bool prefixCategory = true;
        /// Let recorded messages continue to the handler installed before the dispatcher.
        bool forwardToPreviousHandler = false;
        /// Hide messages from captures created before this one, for example to
        /// silence an enclosing QML warning scope.
        bool suppressOuterCaptures = false;
    };

    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg) : LogCapture(Options{.minimumType = minimumType}) {}

    explicit LogCapture(Options options) : m_options(std::move(options))
    {
        QMutexLocker lock(&s_mutex);
        if (s_captures.isEmpty()) {
            detail::MessageLogDispatcher::subscribe(&s_subscriptionToken, &LogCapture::handler);
        }
        s_captures.push_back(this);
    }

    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;
    LogCapture(LogCapture&&) = delete;
    LogCapture& operator=(LogCapture&&) = delete;

    ~LogCapture()
    {
        bool shouldUnsubscribe = false;
        QMutexLocker lock(&s_mutex);
        s_captures.removeAll(this);
        if (s_captures.isEmpty()) {
            shouldUnsubscribe = true;
        }
        lock.unlock();

        if (shouldUnsubscribe) {
            detail::MessageLogDispatcher::unsubscribe(&s_subscriptionToken, &LogCapture::handler);
        }
    }

    [[nodiscard]] QStringList messages() const
    {
        QMutexLocker lock(&s_mutex);
        return m_messages;
    }

    [[nodiscard]] QString joinedMessages() const
    {
        return messages().join('\n');
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

    [[nodiscard]] static QString categoryName(const QMessageLogContext& context)
    {
        return QString::fromUtf8(context.category != nullptr ? context.category : "default");
    }

    [[nodiscard]] bool records(const QtMsgType type, const QMessageLogContext& context) const
    {
        if (severityRank(type) < severityRank(m_options.minimumType)) {
            return false;
        }

        return context.category == nullptr || !m_options.ignoredCategories.contains(categoryName(context));
    }

    [[nodiscard]] QString formatMessage(const QMessageLogContext& context, const QString& message) const
    {
        if (!m_options.prefixCategory) {
            return message;
        }

        return QStringLiteral("%1 %2").arg(categoryName(context), message);
    }

    static detail::MessageLogDispatcher::Result
    handler(void* /*owner*/, const QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        QMutexLocker lock(&s_mutex);
        bool forwardToPreviousHandler = true;
        // Newest captures are handled first so a suppressing capture can keep
        // the message away from the captures it was created inside of.
        for (qsizetype index = s_captures.size() - 1; index >= 0; --index) {
            LogCapture* capture = s_captures.at(index);
            if (capture->records(type, context)) {
                capture->m_messages.push_back(capture->formatMessage(context, message));
            }

            if (!capture->m_options.forwardToPreviousHandler) {
                forwardToPreviousHandler = false;
            }

            if (capture->m_options.suppressOuterCaptures) {
                break;
            }
        }

        if (forwardToPreviousHandler) {
            return detail::MessageLogDispatcher::Result::Continue;
        }
        return detail::MessageLogDispatcher::Result::Stop;
    }

    Options m_options;
    QStringList m_messages;
    static inline QMutex s_mutex;
    static inline QList<LogCapture*> s_captures;
    static inline int s_subscriptionToken = 0;
};

}  // namespace skygate::testsupport
