#pragma once

#include "SkyPersistedStateSnapshot.hpp"

namespace skygate::ui::internal {

struct SkyTimelineState final {
    bool live = true;
    bool toolbarCollapsed = false;
    bool catchingUpToCurrentUtc = false;
    double speedMultiplier = 1.0;
    double speedRemainderSeconds = 0.0;
    int stepSeconds = SkyPersistedStateSnapshot::kDefaultStepSeconds;
};

}  // namespace skygate::ui::internal
