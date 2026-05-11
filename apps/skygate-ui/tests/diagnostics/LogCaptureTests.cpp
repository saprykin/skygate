#include "LogCapture.hpp"
#include "QmlWarningScope.hpp"

#include <QtTest/QtTest>

#include <QDebug>
#include <QMessageLogContext>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <optional>

namespace {

QStringList s_forwardedWarnings;
int s_recursiveHandlerCalls = 0;

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

void recordAndRecurseOnce(
    const QtMsgType type,
    const QMessageLogContext& /*context*/,
    const QString& message
)
{
    if (type != QtWarningMsg) {
        return;
    }

    ++s_recursiveHandlerCalls;
    s_forwardedWarnings.push_back(message);
    if (message == QStringLiteral("root-forwarding-warning")) {
        qWarning().noquote() << "recursive-warning";
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
    void logCaptureFiltersByMinimumSeverity();
    void nestedLogCaptureFanOutsAndRestoresPreviousHandler();
    void interleavedHelpersRestorePreviousHandler();
    void temporaryHandlerReplacementInsideScopeRestoresDispatcher();
    void nestedQmlWarningScopesForwardThroughEnabledScopes();
    void recursiveForwardingHandlerDoesNotReenterDispatcher();
    void disabledQmlWarningScopeStopsForwardingToOuterScopes();
};

void LogCaptureTests::logCaptureFiltersByMinimumSeverity()
{
    skygate::ui::tests::LogCapture warningCapture;
    skygate::ui::tests::LogCapture infoCapture(QtInfoMsg);

    qInfo().noquote() << "threshold-info";
    qWarning().noquote() << "threshold-warning";

    const QString warningMessages = warningCapture.joinedMessages();
    const QString infoMessages = infoCapture.joinedMessages();
    QVERIFY(!warningMessages.contains(QStringLiteral("threshold-info")));
    QVERIFY(warningMessages.contains(QStringLiteral("threshold-warning")));
    QVERIFY(infoMessages.contains(QStringLiteral("threshold-info")));
    QVERIFY(infoMessages.contains(QStringLiteral("threshold-warning")));
}

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

void LogCaptureTests::interleavedHelpersRestorePreviousHandler()
{
    s_forwardedWarnings.clear();
    MessageHandlerGuard handlerGuard(&recordWarnings);

    std::optional<skygate::ui::tests::QmlWarningScope> warnings;
    std::optional<skygate::ui::tests::LogCapture> capture;

    warnings.emplace();
    capture.emplace();
    warnings.reset();

    qWarning().noquote() << "captured-after-interleaved-reset";
    QVERIFY(capture->joinedMessages().contains(QStringLiteral("captured-after-interleaved-reset")));
    QVERIFY(s_forwardedWarnings.isEmpty());

    capture.reset();

    qWarning().noquote() << "after-interleaved-helpers";
    QCOMPARE(s_forwardedWarnings, QStringList({QStringLiteral("after-interleaved-helpers")}));
}

void LogCaptureTests::temporaryHandlerReplacementInsideScopeRestoresDispatcher()
{
    s_forwardedWarnings.clear();
    MessageHandlerGuard handlerGuard(&recordWarnings);

    QStringList scopeMessages;
    {
        const skygate::ui::tests::QmlWarningScope warnings;
        {
            MessageHandlerGuard replacementGuard(&recordWarnings);
            qWarning().noquote() << "temporary-replacement-warning";
        }

        qWarning().noquote() << "after-temporary-replacement";
        scopeMessages = warnings.messages();
    }

    QVERIFY(containsMessage(scopeMessages, QStringLiteral("after-temporary-replacement")));
    QVERIFY(!containsMessage(scopeMessages, QStringLiteral("temporary-replacement-warning")));
    QCOMPARE(
        s_forwardedWarnings,
        QStringList({
            QStringLiteral("temporary-replacement-warning"),
            QStringLiteral("after-temporary-replacement"),
        })
    );
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

void LogCaptureTests::recursiveForwardingHandlerDoesNotReenterDispatcher()
{
    s_forwardedWarnings.clear();
    s_recursiveHandlerCalls = 0;
    MessageHandlerGuard handlerGuard(&recordAndRecurseOnce);

    QStringList scopeMessages;
    {
        const skygate::ui::tests::QmlWarningScope warnings;
        qWarning().noquote() << "root-forwarding-warning";
        scopeMessages = warnings.messages();
    }

    QVERIFY(containsMessage(scopeMessages, QStringLiteral("root-forwarding-warning")));
    QVERIFY(!containsMessage(scopeMessages, QStringLiteral("recursive-warning")));
    QCOMPARE(s_forwardedWarnings, QStringList({QStringLiteral("root-forwarding-warning")}));
    QCOMPARE(s_recursiveHandlerCalls, 1);
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
