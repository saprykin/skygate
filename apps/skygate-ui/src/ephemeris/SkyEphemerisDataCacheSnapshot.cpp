#include "SkyEphemerisDataCacheSnapshot.hpp"

#include <QStringList>

#include <optional>

namespace {

// Metadata text is only consulted for snapshots stored before the range was
// recorded; it names the kernel model in the asset id, profile id, or version.
QString installedKernelMetadataText(const SkyEphemerisDataCacheSnapshot& snapshot)
{
    return QStringList{
        snapshot.installedKernelAssetId,
        snapshot.installedKernelProfileId,
        snapshot.installedKernelVersion,
    }
        .join(QLatin1Char(' '))
        .toLower();
}

}  // namespace

std::optional<bool> SkyEphemerisDataCacheSnapshot::legacyKernelLongRange(const SkyEphemerisDataCacheSnapshot& snapshot)
{
    const QString metadataText = installedKernelMetadataText(snapshot);
    if (metadataText.contains(QStringLiteral("de441")) || metadataText.contains(QStringLiteral("long"))) {
        return true;
    }
    if (metadataText.contains(QStringLiteral("de440"))) {
        return false;
    }
    return std::nullopt;
}
