#include "LogCapture.hpp"
#include "QmlWarningScope.hpp"

#include <QtTest/QtTest>

#include <QDebug>
#include <QMessageLogContext>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>

namespace {

QStringList s_forwardedWarnings;

void recordWarnings(
    const QtMsgType type,
    const QMessageLogContext& /*context*/,
    const QString& message
)
{
    if (type == QtWarningMsg) {
        s_forwardedWarnings.push_back(message);
    }
}

class MessageHandlerGuard final {
public:
    explicit MessageHandlerGuard(const QtMessageHandler handler) :
        m_previousHandler(qInstallMessageHandler(handler))
    {
    }

    ~MessageHandlerGuard()
    {
        qInstallMessageHandler(m_previousHandler);
    }

private:
    QtMessageHandler m_previousHandler = nullptr;
};

[[nodiscard]] bool containsMessage(const QStringList& messages, const QString& expected)
{
    return std::any_of(messages.cbegin(), messages.cend(), [&expected](const QString& message) {
        return message.contains(expected);
    });
}

}  // namespace

class LogCaptureTests final : public QObject {
    Q_OBJECT

private slots:
    void nestedLogCaptureFanOutsAndRestoresPreviousHandler();
    void nestedQmlWarningScopesForwardThroughEnabledScopes();
    void disabledQmlWarningScopeStopsForwardingToOuterScopes();
};

void LogCaptureTests::nestedLogCaptureFanOutsAndRestoresPreviousHandler()
{
    s_forwardedWarnings.clear();
    MessageHandlerGuard handlerGuard(&recordWarnings);

    {
        skygate::ui::tests::LogCapture outer;
        qWarning().noquote() << "outer-before";

        {
            skygate::ui::tests::LogCapture inner;
            qWarning().noquote() << "nested-warning";
            QVERIFY(inner.joinedMessages().contains(QStringLiteral("nested-warning")));
        }

        qWarning().noquote() << "outer-after";

        const QString outerMessages = outer.joinedMessages();
        QVERIFY(outerMessages.contains(QStringLiteral("outer-before")));
        QVERIFY(outerMessages.contains(QStringLiteral("nested-warning")));
        QVERIFY(outerMessages.contains(QStringLiteral("outer-after")));
        QVERIFY(s_forwardedWarnings.isEmpty());
    }

    qWarning().noquote() << "after-capture";
    QCOMPARE(s_forwardedWarnings, QStringList({QStringLiteral("after-capture")}));
}

void LogCaptureTests::nestedQmlWarningScopesForwardThroughEnabledScopes()
{
    s_forwardedWarnings.clear();
    MessageHandlerGuard handlerGuard(&recordWarnings);

    QStringList outerMessages;
    QStringList innerMessages;

    {
        const skygate::ui::tests::QmlWarningScope outer;
        {
            const skygate::ui::tests::QmlWarningScope inner;
            qWarning().noquote() << "nested-qml-warning";
            innerMessages = inner.messages();
        }
        outerMessages = outer.messages();
    }

    QVERIFY(containsMessage(innerMessages, QStringLiteral("nested-qml-warning")));
    QVERIFY(containsMessage(outerMessages, QStringLiteral("nested-qml-warning")));
    QCOMPARE(s_forwardedWarnings, QStringList({QStringLiteral("nested-qml-warning")}));

    qWarning().noquote() << "after-qml-scope";
    QCOMPARE(
        s_forwardedWarnings,
        QStringList({
            QStringLiteral("nested-qml-warning"),
            QStringLiteral("after-qml-scope"),
        })
    );
}

void LogCaptureTests::disabledQmlWarningScopeStopsForwardingToOuterScopes()
{
    s_forwardedWarnings.clear();
    MessageHandlerGuard handlerGuard(&recordWarnings);

    QStringList outerMessages;
    QStringList innerMessages;

    {
        const skygate::ui::tests::QmlWarningScope outer(
            skygate::ui::tests::QmlWarningScope::Forwarding::Disabled
        );
        {
            const skygate::ui::tests::QmlWarningScope inner(
                skygate::ui::tests::QmlWarningScope::Forwarding::Disabled
            );
            qWarning().noquote() << "suppressed-nested-qml-warning";
            innerMessages = inner.messages();
        }

        qWarning().noquote() << "outer-only-qml-warning";
        outerMessages = outer.messages();
    }

    QVERIFY(containsMessage(innerMessages, QStringLiteral("suppressed-nested-qml-warning")));
    QVERIFY(!containsMessage(outerMessages, QStringLiteral("suppressed-nested-qml-warning")));
    QVERIFY(containsMessage(outerMessages, QStringLiteral("outer-only-qml-warning")));
    QVERIFY(s_forwardedWarnings.isEmpty());
}

QTEST_APPLESS_MAIN(LogCaptureTests)

#include "LogCaptureTests.moc"
