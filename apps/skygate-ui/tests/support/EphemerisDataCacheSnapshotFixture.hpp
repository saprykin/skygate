#pragma once

#include "SkySettingsStore.hpp"

#include <QString>

namespace skygate::ui::tests {

// Installed-data cache snapshots shared by the ephemeris data manager suites.
class EphemerisDataCacheSnapshotFixture final {
public:
    [[nodiscard]] static SkySettingsStore::EphemerisDataCacheSnapshot installedSnapshot(
        const QString& kernelPath,
        const QString& earthOrientationPath,
        const QString& leapSecondTablePath = {},
        const QString& deltaTDataPath = {}
    );
};

}  // namespace skygate::ui::tests
