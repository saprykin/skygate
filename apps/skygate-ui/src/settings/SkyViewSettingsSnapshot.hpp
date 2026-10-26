#pragma once

#include "SkyTimelineViewDefaultsPolicy.hpp"

#include <QString>

namespace skygate::ui::internal {

struct SkyViewSettingsSnapshot final {
    double magnitudeCutoff = SkyTimelineViewDefaultsPolicy::kDefaultMagnitudeCutoff;
    double centerAltitudeDeg = SkyTimelineViewDefaultsPolicy::kLegacyViewCenterAltitudeDeg;
    double centerAzimuthDeg = SkyTimelineViewDefaultsPolicy::kLegacyViewCenterAzimuthDeg;
    double fieldOfViewDeg = SkyTimelineViewDefaultsPolicy::kDefaultFieldOfViewDeg;
    QString projectionTypeText;
    QString themeId;
};

}  // namespace skygate::ui::internal
