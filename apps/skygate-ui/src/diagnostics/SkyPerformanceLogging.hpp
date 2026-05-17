#pragma once

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QtGlobal>

namespace skygate::ui {

Q_DECLARE_LOGGING_CATEGORY(skygatePerfLog)

[[nodiscard]] bool performanceLoggingEnabled() noexcept;
void startPerformanceTimer(QElapsedTimer& timer);
[[nodiscard]] qint64 performanceElapsedNanoseconds(const QElapsedTimer& timer) noexcept;
[[nodiscard]] double performanceMilliseconds(qint64 nanoseconds) noexcept;
[[nodiscard]] double performanceElapsedMilliseconds(const QElapsedTimer& timer) noexcept;

}  // namespace skygate::ui
