#pragma once

#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"

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
    double atmosphericPressureHpa = skygate::ephemeris::EphemerisEngineOptions::kDefaultAtmosphericPressureHpa;
    double atmosphericTemperatureC = skygate::ephemeris::EphemerisEngineOptions::kDefaultAtmosphericTemperatureC;
    double relativeHumidity = skygate::ephemeris::EphemerisEngineOptions::kDefaultRelativeHumidity;
    double observingWavelengthMicrometers =
        skygate::ephemeris::EphemerisEngineOptions::kDefaultObservingWavelengthMicrometers;
    QString preferredDataProfileId = QStringLiteral("de440s-short-range");
    bool onlineUpdatesEnabled = true;
    QString updatePresetId = QStringLiteral("bundled");
    QString updateManifestUrl;
};
