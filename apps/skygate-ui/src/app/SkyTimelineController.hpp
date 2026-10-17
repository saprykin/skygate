#pragma once

#include "SkyTimelineState.hpp"

namespace skygate::ui::internal {

class SkyTimelineController final {
public:
    [[nodiscard]] bool live() const noexcept;
    [[nodiscard]] bool toolbarCollapsed() const noexcept;
    [[nodiscard]] double speedMultiplier() const noexcept;
    [[nodiscard]] int stepSeconds() const noexcept;
    [[nodiscard]] bool catchingUpToCurrentUtc() const noexcept;

    [[nodiscard]] bool setLive(bool live) noexcept;
    [[nodiscard]] bool setToolbarCollapsed(bool toolbarCollapsed) noexcept;
    [[nodiscard]] bool setSpeedMultiplier(double speedMultiplier) noexcept;
    [[nodiscard]] bool setStepSeconds(int stepSeconds) noexcept;
    void setCatchingUpToCurrentUtc(bool catchingUpToCurrentUtc) noexcept;
    void startLiveAtCurrentUtc() noexcept;

private:
    SkyTimelineState m_state;
};

}  // namespace skygate::ui::internal
