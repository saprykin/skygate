#include "SkyLiveClock.hpp"

#include <chrono>

void SkyLiveClock::start(const skygate::core::UtcTimePoint& anchorUtc)
{
    m_anchorUtc = anchorUtc;
    m_elapsedTimer.start();
}

void SkyLiveClock::stop()
{
    if (isRunning()) {
        m_anchorUtc = currentUtc();
        m_elapsedTimer.invalidate();
    }
}

bool SkyLiveClock::isRunning() const noexcept
{
    return m_elapsedTimer.isValid();
}

skygate::core::UtcTimePoint SkyLiveClock::currentUtc() const
{
    if (!isRunning()) {
        return m_anchorUtc;
    }

    return m_anchorUtc
           + std::chrono::duration_cast<skygate::core::UtcTimePoint::duration>(
               std::chrono::nanoseconds(m_elapsedTimer.nsecsElapsed())
           );
}

void SkyLiveClock::resetAnchor(const skygate::core::UtcTimePoint& anchorUtc)
{
    m_anchorUtc = anchorUtc;
    if (isRunning()) {
        m_elapsedTimer.restart();
    }
}
