#pragma once

#include "SkyPersistedStateSnapshot.hpp"

#include <QtGlobal>

namespace skygate::ui::internal {

struct SkyTimelineSettingsSnapshot final {
    bool live = true;
    bool toolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = SkyPersistedStateSnapshot::kDefaultStepSeconds;
    qint64 utcEpochMicros = 0;
};

}  // namespace skygate::ui::internal
