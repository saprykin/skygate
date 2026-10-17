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

void SkyTimelineController::startLiveAtCurrentUtc() noexcept
{
    m_state.live = true;
    m_state.catchingUpToCurrentUtc = false;
}

}  // namespace skygate::ui::internal
