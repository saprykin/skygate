#pragma once

#include "SkyEphemerisUserSettingsSnapshot.hpp"
#include "SkyTimelineViewDefaultsPolicy.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"

#include <QString>
#include <QtGlobal>

// The user-facing state restored at startup: timeline, view, location,
// display, catalog selection, logging, and ephemeris preferences.
struct SkyPersistedStateSnapshot final {
    // Version stamped on the persisted settings group.
    static constexpr int kSettingsVersion = 3;
    // The values live in SkyTimelineViewDefaultsPolicy; these names stay
    // part of the aggregate interface.
    static constexpr int kDefaultStepSeconds =
        skygate::ui::internal::SkyTimelineViewDefaultsPolicy::kDefaultStepSeconds;
    static constexpr double kDefaultMagnitudeCutoff =
        skygate::ui::internal::SkyTimelineViewDefaultsPolicy::kDefaultMagnitudeCutoff;
    static constexpr double kDefaultFieldOfViewDeg =
        skygate::ui::internal::SkyTimelineViewDefaultsPolicy::kDefaultFieldOfViewDeg;

    bool live = true;
    bool timelineToolbarCollapsed = false;
    bool searchToolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = kDefaultStepSeconds;
    double magnitudeCutoff = kDefaultMagnitudeCutoff;
    double viewCenterAltitudeDeg = skygate::ui::internal::SkyTimelineViewDefaultsPolicy::kLegacyViewCenterAltitudeDeg;
    double viewCenterAzimuthDeg = skygate::ui::internal::SkyTimelineViewDefaultsPolicy::kLegacyViewCenterAzimuthDeg;
    double viewFieldOfViewDeg = kDefaultFieldOfViewDeg;
    qint64 utcEpochMicros = 0;
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double elevationMeters = 0.0;
    QString locationSourceText;
    QString selectedCityId;
    QString displayTimeZoneId;
    QString projectionTypeText;
    QString themeId;
    SkyOverlayLayerVisibility overlayLayers;
    int catalogPresetIndex = 0;
    QString catalogUrlText;
    int deepSkyCatalogPresetIndex = 0;
    QString deepSkyCatalogUrlText;
    bool logToTerminal = true;
    bool logToFile = false;
    QString logFilePath;
    SkyEphemerisUserSettingsSnapshot ephemeris;
    bool ephemerisSettingsPresent = false;
};
