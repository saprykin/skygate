#pragma once

#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"

#include <QString>

// The ephemeris engine, correction, atmosphere, and update preferences the
// user selected. Persisted with the rest of the settings state.
struct SkyEphemerisUserSettingsSnapshot final {
    skygate::ephemeris::EphemerisEngineKind::Type engineKind = skygate::ephemeris::EphemerisEngineKind::Type::Simple;
    skygate::ephemeris::EphemerisCorrectionFlags correctionFlags =
        skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric();
    QString correctionPresetId = QStringLiteral("apparent-topocentric");
    bool fallbackToSimpleEngine = true;
    bool refractionEnabled = true;
    double atmosphericPressureHpa = 1013.25;
    double atmosphericTemperatureC = 10.0;
    double relativeHumidity = 0.0;
    double observingWavelengthMicrometers = 0.55;
    QString preferredDataProfileId = QStringLiteral("de440s-short-range");
    bool onlineUpdatesEnabled = true;
    QString updatePresetId = QStringLiteral("bundled");
    QString updateManifestUrl;
};
