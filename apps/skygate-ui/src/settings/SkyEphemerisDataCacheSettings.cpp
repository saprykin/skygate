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

QString ephemerisDataCacheKey(const char* name)
{
    return SkySettingsKeyBuilder::key(QStringLiteral("ephemerisData/%1").arg(QString::fromUtf8(name)));
}

// Reads the range stored alongside the installed kernel. Settings written
// before the range was recorded have no value here, which leaves that case to
// the kernel metadata. An unreadable value is reported and treated the same.
std::optional<bool> readInstalledKernelLongRange(QSettings& settings)
{
    const QString key = ephemerisDataCacheKey("installedKernelLongRange");
    if (!settings.contains(key)) {
        return std::nullopt;
    }

    if (const std::optional<bool> storedRange = SkySettingsValueCodecs::boolFromSettingValue(settings.value(key))) {
        return storedRange;
    }
    qCWarning(skygateEphemerisDataCacheLog).noquote()
        << "Invalid boolean setting" << key << "value" << settings.value(key).toString()
        << "- reading the range from the installed kernel metadata";
    return std::nullopt;
}

}  // namespace

bool SkyEphemerisDataCacheSettings::save(const SkyEphemerisDataCacheSnapshot& snapshot)
{
    QSettings settings;
    settings.setValue(SkySettingsKeyBuilder::key("version"), SkyPersistedStateSnapshot::kSettingsVersion);
    settings.setValue(ephemerisDataCacheKey("installedKernelAssetId"), snapshot.installedKernelAssetId);
    settings.setValue(ephemerisDataCacheKey("installedKernelProfileId"), snapshot.installedKernelProfileId);
    settings.setValue(ephemerisDataCacheKey("installedKernelPath"), snapshot.installedKernelPath);
    settings.setValue(ephemerisDataCacheKey("installedKernelVersion"), snapshot.installedKernelVersion);
    if (snapshot.installedKernelLongRange.has_value()) {
        settings.setValue(ephemerisDataCacheKey("installedKernelLongRange"), *snapshot.installedKernelLongRange);
    } else {
        settings.remove(ephemerisDataCacheKey("installedKernelLongRange"));
    }
    settings.setValue(ephemerisDataCacheKey("installedEarthOrientationPath"), snapshot.installedEarthOrientationPath);
    settings.setValue(
        ephemerisDataCacheKey("installedEarthOrientationVersion"), snapshot.installedEarthOrientationVersion
    );
    settings.setValue(ephemerisDataCacheKey("installedLeapSecondTablePath"), snapshot.installedLeapSecondTablePath);
    settings.setValue(
        ephemerisDataCacheKey("installedLeapSecondTableVersion"), snapshot.installedLeapSecondTableVersion
    );
    settings.setValue(ephemerisDataCacheKey("installedDeltaTDataPath"), snapshot.installedDeltaTDataPath);
    settings.setValue(ephemerisDataCacheKey("installedDeltaTDataVersion"), snapshot.installedDeltaTDataVersion);
    settings.setValue(
        ephemerisDataCacheKey("dataRevisionToken"),
        snapshot.dataRevisionToken.trimmed().isEmpty() ? SkyEphemerisDataCacheSnapshot{}.dataRevisionToken
                                                       : snapshot.dataRevisionToken.trimmed()
    );
    settings.setValue(
        ephemerisDataCacheKey("lastUpdateResult"),
        snapshot.lastUpdateResult.trimmed().isEmpty() ? SkyEphemerisDataCacheSnapshot{}.lastUpdateResult
                                                      : snapshot.lastUpdateResult.trimmed()
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
    snapshot.installedKernelAssetId = settings.value(ephemerisDataCacheKey("installedKernelAssetId")).toString();
    snapshot.installedKernelProfileId = settings.value(ephemerisDataCacheKey("installedKernelProfileId")).toString();
    snapshot.installedKernelPath = settings.value(ephemerisDataCacheKey("installedKernelPath")).toString();
    snapshot.installedKernelVersion = settings.value(ephemerisDataCacheKey("installedKernelVersion")).toString();
    snapshot.installedKernelLongRange = readInstalledKernelLongRange(settings);
    if (!snapshot.installedKernelLongRange.has_value()) {
        snapshot.installedKernelLongRange = SkyEphemerisDataCacheSnapshot::legacyKernelLongRange(snapshot);
    }
    snapshot.installedEarthOrientationPath =
        settings.value(ephemerisDataCacheKey("installedEarthOrientationPath")).toString();
    snapshot.installedEarthOrientationVersion =
        settings.value(ephemerisDataCacheKey("installedEarthOrientationVersion")).toString();
    snapshot.installedLeapSecondTablePath =
        settings.value(ephemerisDataCacheKey("installedLeapSecondTablePath")).toString();
    snapshot.installedLeapSecondTableVersion =
        settings.value(ephemerisDataCacheKey("installedLeapSecondTableVersion")).toString();
    snapshot.installedDeltaTDataPath = settings.value(ephemerisDataCacheKey("installedDeltaTDataPath")).toString();
    snapshot.installedDeltaTDataVersion =
        settings.value(ephemerisDataCacheKey("installedDeltaTDataVersion")).toString();
    snapshot.dataRevisionToken = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, ephemerisDataCacheKey("dataRevisionToken"), snapshot.dataRevisionToken
    );
    snapshot.lastUpdateResult = SkySettingsValueCodecs::readNonBlankStringSetting(
        settings, ephemerisDataCacheKey("lastUpdateResult"), snapshot.lastUpdateResult
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
