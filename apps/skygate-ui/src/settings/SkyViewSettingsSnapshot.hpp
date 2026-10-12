#pragma once

#include "SkyPersistedStateSnapshot.hpp"

#include <QString>

namespace skygate::ui::internal {

struct SkyViewSettingsSnapshot final {
    double magnitudeCutoff = SkyPersistedStateSnapshot::kDefaultMagnitudeCutoff;
    double centerAltitudeDeg = SkyPersistedStateSnapshot::kLegacyViewCenterAltitudeDeg;
    double centerAzimuthDeg = SkyPersistedStateSnapshot::kLegacyViewCenterAzimuthDeg;
    double fieldOfViewDeg = SkyPersistedStateSnapshot::kDefaultFieldOfViewDeg;
    QString projectionTypeText;
    QString themeId;
};

}  // namespace skygate::ui::internal
