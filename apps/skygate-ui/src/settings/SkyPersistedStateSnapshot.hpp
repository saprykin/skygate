#pragma once

#include "SkyEphemerisUserSettingsSnapshot.hpp"
#include "theme/SkyOverlayLayerVisibility.hpp"

#include <QString>
#include <QtGlobal>

// The user-facing state restored at startup: timeline, view, location,
// display, catalog selection, logging, and ephemeris preferences.
struct SkyPersistedStateSnapshot final {
    bool live = true;
    bool timelineToolbarCollapsed = false;
    bool searchToolbarCollapsed = false;
    double speedMultiplier = 1.0;
    int stepSeconds = 60;
    double magnitudeCutoff = 6.0;
    double viewCenterAltitudeDeg = 0.0;
    double viewCenterAzimuthDeg = 0.0;
    double viewFieldOfViewDeg = 100.0;
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
