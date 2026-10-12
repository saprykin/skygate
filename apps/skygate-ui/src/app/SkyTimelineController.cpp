#include "SkyTimelineController.hpp"

#include <cmath>

namespace skygate::ui::internal {

bool SkyTimelineController::live() const noexcept
{
    return m_state.live;
}

bool SkyTimelineController::toolbarCollapsed() const noexcept
{
    return m_state.toolbarCollapsed;
}

double SkyTimelineController::speedMultiplier() const noexcept
{
    return m_state.speedMultiplier;
}

int SkyTimelineController::stepSeconds() const noexcept
{
    return m_state.stepSeconds;
}

bool SkyTimelineController::catchingUpToCurrentUtc() const noexcept
{
    return m_state.catchingUpToCurrentUtc;
}

bool SkyTimelineController::setLive(const bool live) noexcept
{
    if (m_state.live == live) {
        return false;
    }

    m_state.live = live;
    resetSpeedProgress();
    m_state.catchingUpToCurrentUtc = false;
    return true;
}

bool SkyTimelineController::setToolbarCollapsed(const bool toolbarCollapsed) noexcept
{
    if (m_state.toolbarCollapsed == toolbarCollapsed) {
        return false;
    }

    m_state.toolbarCollapsed = toolbarCollapsed;
    return true;
}

bool SkyTimelineController::setSpeedMultiplier(const double speedMultiplier) noexcept
{
    if (std::abs(m_state.speedMultiplier - speedMultiplier) < 1e-9) {
        return false;
    }

    m_state.speedMultiplier = speedMultiplier;
    resetSpeedProgress();
    return true;
}

bool SkyTimelineController::setStepSeconds(const int stepSeconds) noexcept
{
    if (m_state.stepSeconds == stepSeconds) {
        return false;
    }

    m_state.stepSeconds = stepSeconds;
    return true;
}

void SkyTimelineController::setCatchingUpToCurrentUtc(const bool catchingUpToCurrentUtc) noexcept
{
    m_state.catchingUpToCurrentUtc = catchingUpToCurrentUtc;
}

void SkyTimelineController::resetSpeedProgress() noexcept
{
    m_state.speedRemainderSeconds = 0.0;
}

void SkyTimelineController::addSpeedRemainderSeconds(const double seconds) noexcept
{
    m_state.speedRemainderSeconds += seconds;
}

int SkyTimelineController::takeWholeSpeedRemainderSeconds() noexcept
{
    const int wholeSeconds = static_cast<int>(std::floor(m_state.speedRemainderSeconds));
    if (wholeSeconds <= 0) {
        return 0;
    }

    m_state.speedRemainderSeconds -= wholeSeconds;
    return wholeSeconds;
}

void SkyTimelineController::startLiveAtCurrentUtc() noexcept
{
    m_state.live = true;
    m_state.catchingUpToCurrentUtc = false;
    resetSpeedProgress();
}

}  // namespace skygate::ui::internal
