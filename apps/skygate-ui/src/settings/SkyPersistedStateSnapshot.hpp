#pragma once

#include "SkyEphemerisUserSettingsSnapshot.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"

#include <QString>
#include <QtGlobal>

// The user-facing state restored at startup: timeline, view, location,
// display, catalog selection, logging, and ephemeris preferences.
struct SkyPersistedStateSnapshot final {
    // Version stamped on the persisted settings group.
    static constexpr int kSettingsVersion = 3;
    // Defaults shared with the runtime domain state and the per-domain
    // settings snapshots, so each value has one owner.
    static constexpr int kDefaultStepSeconds = 60;
    static constexpr double kDefaultMagnitudeCutoff = 6.0;
    static constexpr double kDefaultFieldOfViewDeg = 100.0;
    // A versioned settings file without the view-center keys restores at the
    // horizon facing north, while a fresh runtime state starts at altitude 45,
    // azimuth 180. The persistence fallback stays a separate constant.
    static constexpr double kLegacyViewCenterAltitudeDeg = 0.0;
    static constexpr double kLegacyViewCenterAzimuthDeg = 0.0;

    bool live = true;
    bool timelineToolbarCollapsed = false;
    bool searchToolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = kDefaultStepSeconds;
    double magnitudeCutoff = kDefaultMagnitudeCutoff;
    double viewCenterAltitudeDeg = kLegacyViewCenterAltitudeDeg;
    double viewCenterAzimuthDeg = kLegacyViewCenterAzimuthDeg;
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
