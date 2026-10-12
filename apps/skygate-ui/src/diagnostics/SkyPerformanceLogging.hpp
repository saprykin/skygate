#pragma once

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QtGlobal>

namespace skygate::ui {

Q_DECLARE_LOGGING_CATEGORY(skygatePerfLog)

class SkyPerformanceLogging final {
public:
    [[nodiscard]] static bool enabled() noexcept;
    static void startTimer(QElapsedTimer& timer);
    [[nodiscard]] static qint64 elapsedNanoseconds(const QElapsedTimer& timer) noexcept;
    [[nodiscard]] static double milliseconds(qint64 nanoseconds) noexcept;
    [[nodiscard]] static double elapsedMilliseconds(const QElapsedTimer& timer) noexcept;
};

}  // namespace skygate::ui
