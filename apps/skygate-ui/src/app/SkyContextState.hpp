#pragma once

#include "IProjection.hpp"
#include "ObservationContext.hpp"
#include "ProjectionType.hpp"
#include "SkyContextControllerSupport.hpp"
#include "math/ViewportMath.hpp"

#include <QString>

#include <memory>

class QGeoPositionInfoSource;

namespace skygate::ui::internal {

struct SkyTimelineState final {
    bool live = true;
    bool toolbarCollapsed = false;
    bool catchingUpToCurrentUtc = false;
    double speedMultiplier = 1.0;
    double speedRemainderSeconds = 0.0;
    int stepSeconds = 60;
};

struct SkyViewState final {
    double magnitudeCutoff = 6.0;
    double centerAltitudeDeg = skygate::core::ViewportMath::kDefaultCenterAltitudeDeg;
    double centerAzimuthDeg = skygate::core::ViewportMath::kDefaultCenterAzimuthDeg;
    double fieldOfViewDeg = 100.0;
    skygate::core::ProjectionType projectionType = skygate::core::ProjectionType::Stereographic;
    std::unique_ptr<skygate::core::IProjection> projection;
};

struct SkyLocationState final {
    skygate::core::ObservationContext context;
    SkyContextLocationSource source = SkyContextLocationSourceCodec::defaultSource();
    QString statusText;
    QString selectedCityId;
    QString selectedCityDisplayText;
    QGeoPositionInfoSource* positionSource = nullptr;
    bool positionSourceConnected = false;
    bool requestLocationPermission = true;
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
