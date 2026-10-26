#pragma once

#include <skygate/testsupport/LogCapture.hpp>

#include <QStringList>
#include <QtGlobal>

#include <utility>

namespace skygate::ui::tests {

/// Collects the warning-level and more severe messages emitted while the scope
/// is alive. Collection is delegated to LogCapture so test support keeps one
/// message dispatcher client.
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
        : m_capture(
              skygate::testsupport::LogCapture::Options{
                  .minimumType = QtWarningMsg,
                  .ignoredCategories = std::move(ignoredCategories),
                  .prefixCategory = false,
                  .forwardToPreviousHandler = forwarding == Forwarding::Enabled,
                  .suppressOuterCaptures = forwarding == Forwarding::Disabled,
              }
          )
    {
    }

    QmlWarningScope(const QmlWarningScope&) = delete;
    QmlWarningScope& operator=(const QmlWarningScope&) = delete;
    QmlWarningScope(QmlWarningScope&&) = delete;
    QmlWarningScope& operator=(QmlWarningScope&&) = delete;

    [[nodiscard]] QStringList messages() const
    {
        return m_capture.messages();
    }

private:
    skygate::testsupport::LogCapture m_capture;
};

}  // namespace skygate::ui::tests
