#include "EphemerisDataCacheSnapshotFixture.hpp"

namespace skygate::ui::tests {

SkyEphemerisDataCacheSnapshot EphemerisDataCacheSnapshotFixture::installedSnapshot(
    const QString& kernelPath,
    const QString& earthOrientationPath,
    const QString& leapSecondTablePath,
    const QString& deltaTDataPath
)
{
    SkyEphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelAssetId = QStringLiteral("de440s-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("de440s-short-range");
    snapshot.installedKernelPath = kernelPath;
    snapshot.installedKernelVersion = QStringLiteral("DE-test");
    snapshot.installedEarthOrientationPath = earthOrientationPath;
    snapshot.installedEarthOrientationVersion = QStringLiteral("EOP-test");
    snapshot.installedLeapSecondTablePath = leapSecondTablePath;
    snapshot.installedLeapSecondTableVersion = QStringLiteral("LS-test");
    snapshot.installedDeltaTDataPath = deltaTDataPath;
    snapshot.installedDeltaTDataVersion = QStringLiteral("DT-test");
    snapshot.dataRevisionToken = QStringLiteral("installed-rev");
    snapshot.lastUpdateResult = QStringLiteral("Installed");
    return snapshot;
}

}  // namespace skygate::ui::tests
