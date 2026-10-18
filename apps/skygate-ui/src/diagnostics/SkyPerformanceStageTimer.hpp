#pragma once

#include <QElapsedTimer>
#include <QtGlobal>

namespace skygate::ui {

// Measures the stages of a build. A stage ends at each stageMilliseconds()
// call, so callers log per-stage durations without keeping absolute marks.
class SkyPerformanceStageTimer final {
public:
    SkyPerformanceStageTimer();

    // Milliseconds since construction or since the previous stage mark.
    [[nodiscard]] double stageMilliseconds() noexcept;
    [[nodiscard]] double elapsedMilliseconds() const noexcept;

private:
    QElapsedTimer m_timer;
    qint64 m_lastStageNanoseconds = 0;
};

}  // namespace skygate::ui
