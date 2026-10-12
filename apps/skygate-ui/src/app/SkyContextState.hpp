#pragma once

#include "IProjection.hpp"
#include "ObservationContext.hpp"
#include "ProjectionType.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkyPersistedStateSnapshot.hpp"
#include "math/ViewportMath.hpp"

#include <QString>

#include <memory>

namespace skygate::ui::internal {

struct SkyTimelineState final {
    bool live = true;
    bool toolbarCollapsed = false;
    bool catchingUpToCurrentUtc = false;
    double speedMultiplier = 1.0;
    double speedRemainderSeconds = 0.0;
    int stepSeconds = SkyPersistedStateSnapshot::kDefaultStepSeconds;
};

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

struct SkyLocationState final {
    skygate::core::ObservationContext context;
    SkyContextLocationSource source = SkyContextLocationSourceCodec::defaultSource();
    QString statusText;
    QString selectedCityId;
    QString selectedCityDisplayText;
};

struct SkySearchState final {
    bool toolbarCollapsed = false;
    QString selectedTargetKind;
    QString selectedTargetId;
    QString trackedTargetKind;
    QString trackedTargetId;
    QString trackedTargetDisplayText;
};

}  // namespace skygate::ui::internal
