#include "SkyEphemerisDataCacheSettings.hpp"
#include "SkyPersistedStateSnapshot.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QLoggingCategory>
#include <QSettings>
#include <QString>

#include <optional>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateEphemerisDataCacheLog, "skygate.ephemeris.cache")

// Reads the range stored alongside the installed kernel. Settings written
// before the range was recorded have no value here, which leaves that case to
// the kernel metadata. An unreadable value is reported and treated the same.
std::optional<bool> readInstalledKernelLongRange(QSettings& settings)
{
    return SkySettingsValueCodecs::readOptionalBoolSetting(
        settings,
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelLongRange"),
        skygateEphemerisDataCacheLog,
        "reading the range from the installed kernel metadata"
    );
}

}  // namespace

bool SkyEphemerisDataCacheSettings::save(const SkyEphemerisDataCacheSnapshot& snapshot)
{
    QSettings settings;
    settings.setValue(SkySettingsKeyBuilder::key("version"), SkyPersistedStateSnapshot::kSettingsVersion);
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelAssetId"), snapshot.installedKernelAssetId
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelProfileId"), snapshot.installedKernelProfileId
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelPath"), snapshot.installedKernelPath
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelVersion"), snapshot.installedKernelVersion
    );
    if (snapshot.installedKernelLongRange.has_value()) {
        settings.setValue(
            SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelLongRange"),
            *snapshot.installedKernelLongRange
        );
    } else {
        settings.remove(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelLongRange"));
    }
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedEarthOrientationPath"),
        snapshot.installedEarthOrientationPath
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedEarthOrientationVersion"),
        snapshot.installedEarthOrientationVersion
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedLeapSecondTablePath"),
        snapshot.installedLeapSecondTablePath
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedLeapSecondTableVersion"),
        snapshot.installedLeapSecondTableVersion
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedDeltaTDataPath"), snapshot.installedDeltaTDataPath
    );
    settings.setValue(
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedDeltaTDataVersion"),
        snapshot.installedDeltaTDataVersion
    );
    SkySettingsValueCodecs::writeNonBlankStringSetting(
        settings,
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "dataRevisionToken"),
        snapshot.dataRevisionToken,
        SkyEphemerisDataCacheSnapshot{}.dataRevisionToken
    );
    SkySettingsValueCodecs::writeNonBlankStringSetting(
        settings,
        SkySettingsKeyBuilder::nestedKey("ephemerisData", "lastUpdateResult"),
        snapshot.lastUpdateResult,
        SkyEphemerisDataCacheSnapshot{}.lastUpdateResult
    );
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateEphemerisDataCacheLog) << "Failed to save ephemeris data cache metadata";
        return false;
    }
    return true;
}

SkyEphemerisDataCacheSnapshot SkyEphemerisDataCacheSettings::load()
{
    QSettings settings;
    SkyEphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelAssetId =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelAssetId")).toString();
    snapshot.installedKernelProfileId =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelProfileId")).toString();
    snapshot.installedKernelPath =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelPath")).toString();
    snapshot.installedKernelVersion =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedKernelVersion")).toString();
    snapshot.installedKernelLongRange = readInstalledKernelLongRange(settings);
    if (!snapshot.installedKernelLongRange.has_value()) {
        snapshot.installedKernelLongRange = SkyEphemerisDataCacheSnapshot::legacyKernelLongRange(snapshot);
    }
    snapshot.installedEarthOrientationPath =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedEarthOrientationPath")).toString();
    snapshot.installedEarthOrientationVersion =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedEarthOrientationVersion"))
            .toString();
    snapshot.installedLeapSecondTablePath =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedLeapSecondTablePath")).toString();
    snapshot.installedLeapSecondTableVersion =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedLeapSecondTableVersion")).toString();
    snapshot.installedDeltaTDataPath =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedDeltaTDataPath")).toString();
    snapshot.installedDeltaTDataVersion =
        settings.value(SkySettingsKeyBuilder::nestedKey("ephemerisData", "installedDeltaTDataVersion")).toString();
    snapshot.dataRevisionToken = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, SkySettingsKeyBuilder::nestedKey("ephemerisData", "dataRevisionToken"), snapshot.dataRevisionToken
    );
    snapshot.lastUpdateResult = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, SkySettingsKeyBuilder::nestedKey("ephemerisData", "lastUpdateResult"), snapshot.lastUpdateResult
    );
    return snapshot;
}

bool SkyEphemerisDataCacheSettings::clear()
{
    QSettings settings;
    settings.remove(SkySettingsKeyBuilder::key("ephemerisData"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateEphemerisDataCacheLog) << "Failed to clear ephemeris data cache metadata";
        return false;
    }
    qCInfo(skygateEphemerisDataCacheLog) << "Ephemeris data cache metadata cleared; bundled fallback is active";
    return true;
}

}  // namespace skygate::ui::internal
