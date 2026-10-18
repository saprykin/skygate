#pragma once

#include <QString>

#include <optional>

// Installed ephemeris data as restored from settings: the selected profile,
// asset paths and versions, and the outcome text of the last update.
struct SkyEphemerisDataCacheSnapshot final {
    QString installedKernelAssetId;
    QString installedKernelProfileId;
    QString installedKernelPath;
    QString installedKernelVersion;
    // True for a long-range kernel, false for a short-range kernel, and unset
    // for metadata written before the range was recorded.
    std::optional<bool> installedKernelLongRange;
    QString installedEarthOrientationPath;
    QString installedEarthOrientationVersion;
    QString installedLeapSecondTablePath;
    QString installedLeapSecondTableVersion;
    QString installedDeltaTDataPath;
    QString installedDeltaTDataVersion;
    QString dataRevisionToken = QStringLiteral("bundled");
    QString lastUpdateResult = QStringLiteral("Bundled fallback");

    // Long-range flag to assume for metadata stored before the range was
    // recorded; the kernel model is named by the asset id, profile id, or
    // version text. Returns no value when that text names no known kernel.
    [[nodiscard]] static std::optional<bool> legacyKernelLongRange(const SkyEphemerisDataCacheSnapshot& snapshot);
};
