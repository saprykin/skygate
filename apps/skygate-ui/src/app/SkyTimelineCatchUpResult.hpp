#pragma once

#include "UtcTimePoint.hpp"

namespace skygate::ui::internal {

struct SkyTimelineCatchUpResult final {
    skygate::core::UtcTimePoint nextUtc;
    bool advancedWhileCatchingUp = false;
    bool catchingUpToCurrentUtc = false;
};

}  // namespace skygate::ui::internal
