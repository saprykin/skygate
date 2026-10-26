#pragma once

#include "settings/SkyTimelineViewDefaultsPolicy.hpp"

namespace skygate::ui::internal {

struct SkyTimelineState final {
    bool live = true;
    bool toolbarCollapsed = false;
    bool catchingUpToCurrentUtc = false;
    double speedMultiplier = 1.0;
    int stepSeconds = SkyTimelineViewDefaultsPolicy::kDefaultStepSeconds;
};

}  // namespace skygate::ui::internal
