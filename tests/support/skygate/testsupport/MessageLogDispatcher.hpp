#pragma once

#include <QList>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QtGlobal>

namespace skygate::testsupport::detail {

class MessageLogDispatcher final {
public:
    enum class Result {
        Continue,
        Stop
    };

    using Callback = Result (*)(
        void* owner,
        QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    );

    MessageLogDispatcher() = delete;

    static void subscribe(void* owner, const Callback callback)
    {
        QMutexLocker lock(&s_mutex);
        if (s_subscriptions.isEmpty()) {
            const QtMessageHandler previousHandler =
                qInstallMessageHandler(&MessageLogDispatcher::messageHandler);
            if (previousHandler != &MessageLogDispatcher::messageHandler) {
                s_previousHandler = previousHandler;
            }
        }
        s_subscriptions.push_back({owner, callback});
    }

    static void unsubscribe(void* owner, const Callback callback)
    {
        QtMessageHandler previousHandler = nullptr;
        bool shouldRestore = false;
        {
            QMutexLocker lock(&s_mutex);
            for (qsizetype index = s_subscriptions.size() - 1; index >= 0; --index) {
                const Subscription& subscription = s_subscriptions.at(index);
                if (subscription.owner == owner && subscription.callback == callback) {
                    s_subscriptions.removeAt(index);
                    break;
                }
            }

            if (s_subscriptions.isEmpty()) {
                previousHandler = s_previousHandler;
                shouldRestore = true;
            }
        }

        if (shouldRestore) {
            restorePreviousHandler(previousHandler);
        }
    }

private:
    struct Subscription final {
        void* owner = nullptr;
        Callback callback = nullptr;
    };

    class HandlingGuard final {
    public:
        explicit HandlingGuard(bool& isHandling) :
            m_isHandling(isHandling)
        {
            m_isHandling = true;
        }

        ~HandlingGuard()
        {
            m_isHandling = false;
        }

    private:
        bool& m_isHandling;
    };

    static void restorePreviousHandler(const QtMessageHandler previousHandler)
    {
        const QtMessageHandler replacedHandler = qInstallMessageHandler(previousHandler);
        if (replacedHandler == &MessageLogDispatcher::messageHandler) {
            QMutexLocker lock(&s_mutex);
            s_previousHandler = nullptr;
            return;
        }

        qInstallMessageHandler(replacedHandler);
    }

    static void messageHandler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (s_isHandling) {
            return;
        }

        QList<Subscription> subscriptions;
        QtMessageHandler previousHandler = nullptr;
        {
            QMutexLocker lock(&s_mutex);
            subscriptions = s_subscriptions;
            previousHandler = s_previousHandler;
        }

        HandlingGuard guard(s_isHandling);
        bool shouldForward = true;
        for (qsizetype index = subscriptions.size() - 1; index >= 0; --index) {
            const Subscription& subscription = subscriptions.at(index);
            if (
                subscription.callback != nullptr
                && subscription.callback(subscription.owner, type, context, message)
                    == Result::Stop
            ) {
                shouldForward = false;
                break;
            }
        }

        if (
            shouldForward
            && previousHandler != nullptr
            && previousHandler != &MessageLogDispatcher::messageHandler
        ) {
            previousHandler(type, context, message);
        }
    }

private:
    static inline QMutex s_mutex;
    static inline QList<Subscription> s_subscriptions;
    static inline QtMessageHandler s_previousHandler = nullptr;
    static inline thread_local bool s_isHandling = false;
};

}  // namespace skygate::testsupport::detail
