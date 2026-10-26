#pragma once

#include "ProjectionType.hpp"
#include "math/ViewportMath.hpp"
#include "settings/SkyTimelineViewDefaultsPolicy.hpp"

namespace skygate::ui::internal {

struct SkyViewState final {
    static constexpr double kMagnitudeCutoffMin = -2.0;
    static constexpr double kMagnitudeCutoffMax = 12.0;

    double magnitudeCutoff = SkyTimelineViewDefaultsPolicy::kDefaultMagnitudeCutoff;
    double centerAltitudeDeg = skygate::core::ViewportMath::kDefaultCenterAltitudeDeg;
    double centerAzimuthDeg = skygate::core::ViewportMath::kDefaultCenterAzimuthDeg;
    double fieldOfViewDeg = SkyTimelineViewDefaultsPolicy::kDefaultFieldOfViewDeg;
    skygate::core::ProjectionType projectionType = skygate::core::ProjectionType::Stereographic;
};

}  // namespace skygate::ui::internal
