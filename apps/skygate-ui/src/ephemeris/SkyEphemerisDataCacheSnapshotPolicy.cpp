#include "SkyEphemerisDataCacheSnapshotPolicy.hpp"
#include "SkyEphemerisDataProfileIds.hpp"

namespace skygate::ui::internal {
namespace {

QString trimmed(const QString& value)
{
    return value.trimmed();
}

}  // namespace

SkyEphemerisDataCacheSnapshot
SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(SkyEphemerisDataCacheSnapshot snapshot)
{
    snapshot.installedKernelAssetId = trimmed(snapshot.installedKernelAssetId);
    snapshot.installedKernelProfileId = SkyEphemerisDataProfileIds::canonical(snapshot.installedKernelProfileId);
    snapshot.installedKernelPath = trimmed(snapshot.installedKernelPath);
    snapshot.installedKernelVersion = trimmed(snapshot.installedKernelVersion);
    snapshot.installedEarthOrientationPath = trimmed(snapshot.installedEarthOrientationPath);
    snapshot.installedEarthOrientationVersion = trimmed(snapshot.installedEarthOrientationVersion);
    snapshot.installedLeapSecondTablePath = trimmed(snapshot.installedLeapSecondTablePath);
    snapshot.installedLeapSecondTableVersion = trimmed(snapshot.installedLeapSecondTableVersion);
    snapshot.installedDeltaTDataPath = trimmed(snapshot.installedDeltaTDataPath);
    snapshot.installedDeltaTDataVersion = trimmed(snapshot.installedDeltaTDataVersion);
    snapshot.dataRevisionToken = trimmed(snapshot.dataRevisionToken);
    snapshot.lastUpdateResult = trimmed(snapshot.lastUpdateResult);
    if (snapshot.dataRevisionToken.isEmpty()) {
        snapshot.dataRevisionToken = SkyEphemerisDataCacheSnapshot{}.dataRevisionToken;
    }
    if (snapshot.lastUpdateResult.isEmpty()) {
        snapshot.lastUpdateResult = SkyEphemerisDataCacheSnapshot{}.lastUpdateResult;
    }
    return snapshot;
}

}  // namespace skygate::ui::internal
