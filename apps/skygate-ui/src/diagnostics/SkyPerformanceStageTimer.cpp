#include "SkyPerformanceStageTimer.hpp"
#include "SkyPerformanceLogging.hpp"

namespace skygate::ui {

SkyPerformanceStageTimer::SkyPerformanceStageTimer()
{
    SkyPerformanceLogging::startTimer(m_timer);
}

double SkyPerformanceStageTimer::stageMilliseconds() noexcept
{
    const qint64 nowNanoseconds = SkyPerformanceLogging::elapsedNanoseconds(m_timer);
    const double stageMs = SkyPerformanceLogging::milliseconds(nowNanoseconds - m_lastStageNanoseconds);
    m_lastStageNanoseconds = nowNanoseconds;
    return stageMs;
}

double SkyPerformanceStageTimer::elapsedMilliseconds() const noexcept
{
    return SkyPerformanceLogging::elapsedMilliseconds(m_timer);
}

}  // namespace skygate::ui
