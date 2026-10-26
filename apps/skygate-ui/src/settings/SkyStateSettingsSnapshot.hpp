#pragma once

#include "SkyCatalogSourceSettingsSnapshot.hpp"
#include "SkyEphemerisUserSettingsSnapshot.hpp"
#include "SkyLocationSettingsSnapshot.hpp"
#include "SkyLoggingSettingsSnapshot.hpp"
#include "SkySearchSettingsSnapshot.hpp"
#include "SkyTimelineSettingsSnapshot.hpp"
#include "SkyViewSettingsSnapshot.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"

namespace skygate::ui::internal {

struct SkyStateSettingsSnapshot final {
    SkyTimelineSettingsSnapshot timeline;
    SkySearchSettingsSnapshot search;
    SkyViewSettingsSnapshot view;
    SkyLocationSettingsSnapshot location;
    SkyOverlayLayerVisibility overlayLayers;
    SkyCatalogSourceSettingsSnapshot catalogSources;
    SkyLoggingSettingsSnapshot logging;
    SkyEphemerisUserSettingsSnapshot ephemeris;
    bool ephemerisSettingsPresent = false;
};

}  // namespace skygate::ui::internal
