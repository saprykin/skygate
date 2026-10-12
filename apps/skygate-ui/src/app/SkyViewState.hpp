#pragma once

#include "IProjection.hpp"
#include "ProjectionType.hpp"
#include "math/ViewportMath.hpp"
#include "settings/SkyPersistedStateSnapshot.hpp"

#include <memory>

namespace skygate::ui::internal {

struct SkyViewState final {
    static constexpr double kMagnitudeCutoffMin = -2.0;
    static constexpr double kMagnitudeCutoffMax = 12.0;

    double magnitudeCutoff = SkyPersistedStateSnapshot::kDefaultMagnitudeCutoff;
    double centerAltitudeDeg = skygate::core::ViewportMath::kDefaultCenterAltitudeDeg;
    double centerAzimuthDeg = skygate::core::ViewportMath::kDefaultCenterAzimuthDeg;
    double fieldOfViewDeg = SkyPersistedStateSnapshot::kDefaultFieldOfViewDeg;
    skygate::core::ProjectionType projectionType = skygate::core::ProjectionType::Stereographic;
    std::unique_ptr<skygate::core::IProjection> projection;
};

}  // namespace skygate::ui::internal
