#pragma once

#include <QString>

// Installed ephemeris data as restored from settings: the selected profile,
// asset paths and versions, and the outcome text of the last update.
struct SkyEphemerisDataCacheSnapshot final {
    QString installedKernelAssetId;
    QString installedKernelProfileId;
    QString installedKernelPath;
    QString installedKernelVersion;
    QString installedEarthOrientationPath;
    QString installedEarthOrientationVersion;
    QString installedLeapSecondTablePath;
    QString installedLeapSecondTableVersion;
    QString installedDeltaTDataPath;
    QString installedDeltaTDataVersion;
    QString dataRevisionToken = QStringLiteral("bundled");
    QString lastUpdateResult = QStringLiteral("Bundled fallback");
};
