#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisSnapshot.hpp"
#include "GeoLocation.hpp"
#include "ObservationContext.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PreparedProjection.hpp"
#include "ProjectionType.hpp"
#include "UtcTimePoint.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"
#include "theme/SkyThemeRenderPalette.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <span>

namespace skygate::ephemeris {
class IEphemerisEngine;
}

struct SkySceneFramePipelineInput final {
    const skygate::ephemeris::IEphemerisEngine* ephemerisEngine = nullptr;
    skygate::core::ObservationContext skyContext;
    std::optional<skygate::ephemeris::EphemerisRequest> ephemerisRequest;
    std::uint64_t catalogRevision = 0;
    skygate::ephemeris::EphemerisEngineKind::Type engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    std::uint64_t engineOptionsRevision = 0;
    std::uint64_t ephemerisDataRevision = 0;
    std::uint64_t earthOrientationDataRevision = 0;
    std::uint64_t leapSecondDataRevision = 0;
    skygate::core::ProjectionType projectionType = skygate::core::ProjectionType::Stereographic;
    double viewCenterAltitudeDeg = 0.0;
    double viewCenterAzimuthDeg = 0.0;
    double viewFieldOfViewDeg = 0.0;
    double magnitudeCutoff = 0.0;
    QString themeId;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
    SkyOverlayLayerVisibility overlayLayers;
    std::span<const skygate::ephemeris::ConstellationLineRef> constellationLineRefs;
    std::span<const skygate::ephemeris::ConstellationAnchorGroup> constellationAnchorGroups;
};
