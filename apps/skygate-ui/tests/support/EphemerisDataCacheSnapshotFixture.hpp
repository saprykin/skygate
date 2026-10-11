#pragma once

#include "ephemeris/SkyEphemerisDataCacheSnapshot.hpp"

#include <QString>

namespace skygate::ui::tests {

// Installed-data cache snapshots shared by the ephemeris data manager suites.
class EphemerisDataCacheSnapshotFixture final {
public:
    [[nodiscard]] static SkyEphemerisDataCacheSnapshot installedSnapshot(
        const QString& kernelPath,
        const QString& earthOrientationPath,
        const QString& leapSecondTablePath = {},
        const QString& deltaTDataPath = {}
    );
};

}  // namespace skygate::ui::tests
