#include "SkyEphemerisDataCacheSnapshotPolicy.hpp"
#include "SkyEphemerisDataProfileIds.hpp"

namespace skygate::ui::internal {

SkyEphemerisDataCacheSnapshot
SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(SkyEphemerisDataCacheSnapshot snapshot)
{
    snapshot.installedKernelAssetId = snapshot.installedKernelAssetId.trimmed();
    snapshot.installedKernelProfileId = SkyEphemerisDataProfileIds::canonical(snapshot.installedKernelProfileId);
    snapshot.installedKernelPath = snapshot.installedKernelPath.trimmed();
    snapshot.installedKernelVersion = snapshot.installedKernelVersion.trimmed();
    snapshot.installedEarthOrientationPath = snapshot.installedEarthOrientationPath.trimmed();
    snapshot.installedEarthOrientationVersion = snapshot.installedEarthOrientationVersion.trimmed();
    snapshot.installedLeapSecondTablePath = snapshot.installedLeapSecondTablePath.trimmed();
    snapshot.installedLeapSecondTableVersion = snapshot.installedLeapSecondTableVersion.trimmed();
    snapshot.installedDeltaTDataPath = snapshot.installedDeltaTDataPath.trimmed();
    snapshot.installedDeltaTDataVersion = snapshot.installedDeltaTDataVersion.trimmed();
    snapshot.dataRevisionToken = snapshot.dataRevisionToken.trimmed();
    snapshot.lastUpdateResult = snapshot.lastUpdateResult.trimmed();
    if (snapshot.dataRevisionToken.isEmpty()) {
        snapshot.dataRevisionToken = SkyEphemerisDataCacheSnapshot{}.dataRevisionToken;
    }
    if (snapshot.lastUpdateResult.isEmpty()) {
        snapshot.lastUpdateResult = SkyEphemerisDataCacheSnapshot{}.lastUpdateResult;
    }
    return snapshot;
}

}  // namespace skygate::ui::internal
