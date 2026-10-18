#include "SkySettingsStore.hpp"
#include "SkyCacheFiles.hpp"
#include "SkySettingsKeyBuilder.hpp"
#include "SkySettingsSnapshotCodecs.hpp"
#include "SkySettingsValueCodecs.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>

#include <optional>

using namespace skygate::ui::internal;

namespace {

Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")
Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")
Q_LOGGING_CATEGORY(skygateEphemerisDataCacheLog, "skygate.ephemeris.cache")

constexpr int kDefaultMainWindowWidth = 1100;
constexpr int kDefaultMainWindowHeight = 760;
constexpr int kMinimumMainWindowWidth = 560;
constexpr int kMinimumMainWindowHeight = 420;

// Legacy two-slot catalog cache files, kept under their original names so an
// older installation still restores.
constexpr const char* kCatalogCacheFileName = "catalog-cache-v2.txt";
constexpr const char* kDeepSkyCatalogCacheFileName = "deep-sky-catalog-cache-v1.txt";
constexpr const char* kCatalogBinaryCacheFileName = "catalog-cache-v3.bin";
constexpr const char* kDeepSkyBinaryCatalogCacheFileName = "deep-sky-catalog-cache-v2.bin";

QString defaultCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kCatalogCacheFileName));
}

QString defaultDeepSkyCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kDeepSkyCatalogCacheFileName));
}

QString defaultCatalogBinaryCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kCatalogBinaryCacheFileName));
}

QString defaultDeepSkyBinaryCatalogCachePath()
{
    return SkyCacheFiles::appDataFilePath(QString::fromUtf8(kDeepSkyBinaryCatalogCacheFileName));
}

QString appSettingsKey(const char* name)
{
    return QStringLiteral("app/%1").arg(QString::fromUtf8(name));
}

QString ephemerisDataCacheKey(const char* name)
{
    return SkySettingsKeyBuilder::key(QStringLiteral("ephemerisData/%1").arg(QString::fromUtf8(name)));
}

QSize normalizedMainWindowSize(const QSize& size) noexcept
{
    return QSize(
        size.width() >= kMinimumMainWindowWidth ? size.width() : kDefaultMainWindowWidth,
        size.height() >= kMinimumMainWindowHeight ? size.height() : kDefaultMainWindowHeight
    );
}

int readWindowDimension(QSettings& settings, const QString& key, const int fallback)
{
    bool ok = false;
    const int value = settings.value(key, fallback).toInt(&ok);
    return ok ? value : fallback;
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

// A missing legacy cache file is first use, while a file that exists but cannot
// be opened is a storage problem worth reporting.
void loadCachePayload(const QString& path, const QString& logNoun, QByteArray& payload)
{
    if (path.isEmpty()) {
        return;
    }

    const SkyCacheFiles::ReadResult result = SkyCacheFiles::readFile(path);
    if (result.state == SkyCacheFiles::ReadState::Ok) {
        payload = result.payload;
        return;
    }
    if (result.state == SkyCacheFiles::ReadState::Unreadable) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Failed to open" << logNoun << path << result.errorString;
    }
}

}  // namespace

bool SkySettingsStore::saveState(const SkyPersistedStateSnapshot& snapshot) const
{
    QSettings settings;
    SkySettingsSnapshotCodecs::saveStateSnapshot(settings, snapshot);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateSettingsLog) << "Failed to save SkyGate settings";
        return false;
    }
    return true;
}

std::optional<SkyPersistedStateSnapshot> SkySettingsStore::loadState() const
{
    QSettings settings;
    return SkySettingsSnapshotCodecs::loadStateSnapshot(settings);
}

bool SkySettingsStore::saveMainWindowSize(const QSize& size) const
{
    QSettings settings;
    const QSize normalizedSize = normalizedMainWindowSize(size);
    settings.setValue(appSettingsKey("mainWindowWidth"), normalizedSize.width());
    settings.setValue(appSettingsKey("mainWindowHeight"), normalizedSize.height());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateSettingsLog) << "Failed to save SkyGate main window size";
        return false;
    }
    return true;
}

QSize SkySettingsStore::loadMainWindowSize() const
{
    QSettings settings;
    return normalizedMainWindowSize(QSize(
        readWindowDimension(settings, appSettingsKey("mainWindowWidth"), kDefaultMainWindowWidth),
        readWindowDimension(settings, appSettingsKey("mainWindowHeight"), kDefaultMainWindowHeight)
    ));
}

SkyLoggingSettingsSnapshot SkySettingsStore::loadLoggingSettings() const
{
    QSettings settings;
    return SkySettingsSnapshotCodecs::loadLoggingSettings(settings);
}

bool SkySettingsStore::clearCatalogCache() const
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkySettingsKeyBuilder::key("catalogCachePath"), defaultCatalogCachePath()).toString();
    const QString defaultPath = defaultCatalogCachePath();

    QStringList cachePaths;
    SkyCacheFiles::appendUniquePath(cachePaths, configuredPath);
    SkyCacheFiles::appendUniquePath(cachePaths, defaultPath);
    SkyCacheFiles::appendUniquePath(cachePaths, defaultCatalogBinaryCachePath());

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries =
            defaultDir.entryList(QStringList{"catalog-cache*"}, QDir::Files | QDir::Readable);
        for (const QString& entryName : matchingEntries) {
            SkyCacheFiles::appendUniquePath(cachePaths, defaultDir.filePath(entryName));
        }
    }

    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));

    settings.remove(SkySettingsKeyBuilder::key("catalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("catalogBinaryCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogSourceLabel"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationLineRefs"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogConstellationCount"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear star catalog cache settings";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote() << "Star catalog cache cleared: files" << cachePaths.size();
    }
    return removedAllCacheFiles;
}

bool SkySettingsStore::clearDeepSkyCatalogCache() const
{
    QSettings settings;
    QStringList cachePaths;
    SkyCacheFiles::appendUniquePath(
        cachePaths,
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), defaultDeepSkyCatalogCachePath())
            .toString()
    );
    SkyCacheFiles::appendUniquePath(cachePaths, defaultDeepSkyCatalogCachePath());
    SkyCacheFiles::appendUniquePath(cachePaths, defaultDeepSkyBinaryCatalogCachePath());

    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyBinaryCatalogCachePath"));
    settings.remove(SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear deep-sky catalog cache settings";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote() << "Deep-sky catalog cache cleared: files" << cachePaths.size();
    }
    return removedAllCacheFiles;
}

bool SkySettingsStore::saveCatalogCache(const SkyCatalogLegacyCacheSnapshot& snapshot) const
{
    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString configuredPath =
        settings.value(SkySettingsKeyBuilder::key("catalogCachePath"), defaultCatalogCachePath()).toString();
    const QString configuredDeepSkyPath =
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), defaultDeepSkyCatalogCachePath())
            .toString();
    const auto writePayload = [](const QString& path, const QByteArray& payload) {
        if (path.isEmpty() || payload.isEmpty()) {
            return true;
        }
        return SkyCacheFiles::writeFileAtomically(path, payload, QStringLiteral("catalog cache"));
    };

    if (!writePayload(configuredPath, snapshot.catalogPayload)) {
        return false;
    }
    if (!writePayload(configuredDeepSkyPath, snapshot.deepSkyCatalogPayload)) {
        return false;
    }
    const QString configuredBinaryPath = defaultCatalogBinaryCachePath();
    const QString configuredDeepSkyBinaryPath = defaultDeepSkyBinaryCatalogCachePath();
    if (!writePayload(configuredBinaryPath, snapshot.catalogBinaryPayload)) {
        return false;
    }
    if (!writePayload(configuredDeepSkyBinaryPath, snapshot.deepSkyBinaryPayload)) {
        return false;
    }
    settings.setValue(SkySettingsKeyBuilder::key("version"), SkyPersistedStateSnapshot::kSettingsVersion);
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("catalogCachePath"), configuredPath);
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), configuredDeepSkyPath);
    }
    if (!snapshot.catalogBinaryPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("catalogBinaryCachePath"), configuredBinaryPath);
        settings.setValue(
            SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"), snapshot.catalogBinarySchemaVersion
        );
    }
    if (!snapshot.deepSkyBinaryPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("deepSkyBinaryCatalogCachePath"), configuredDeepSkyBinaryPath);
    }
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("catalogSourceLabel"), snapshot.sourceLabel);
        settings.setValue(SkySettingsKeyBuilder::key("catalogConstellationLineRefs"), snapshot.constellationLineRows);
        settings.setValue(
            SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups"), snapshot.constellationAnchorGroupRows
        );
        settings.setValue(
            SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"), snapshot.constellationLineSchemaVersion
        );
        settings.setValue(
            SkySettingsKeyBuilder::key("catalogConstellationCount"),
            static_cast<qulonglong>(snapshot.constellationCount)
        );
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"), snapshot.deepSkySourceLabel);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to save catalog cache settings";
        return false;
    }
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog cache saved: starBytes" << snapshot.catalogPayload.size()
                                             << "deepSkyBytes" << snapshot.deepSkyCatalogPayload.size()
                                             << "constellationSegments" << snapshot.constellationLineRows.count('\n');
    return true;
}

std::optional<SkyCatalogLegacyCacheSnapshot> SkySettingsStore::loadCatalogCache() const
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkySettingsKeyBuilder::key("catalogCachePath"), defaultCatalogCachePath()).toString();

    SkyCatalogLegacyCacheSnapshot snapshot;
    loadCachePayload(configuredPath, QStringLiteral("star catalog cache"), snapshot.catalogPayload);

    const QString configuredDeepSkyPath =
        settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogCachePath"), defaultDeepSkyCatalogCachePath())
            .toString();
    loadCachePayload(configuredDeepSkyPath, QStringLiteral("deep-sky catalog cache"), snapshot.deepSkyCatalogPayload);

    const QString configuredBinaryPath = defaultCatalogBinaryCachePath();
    loadCachePayload(configuredBinaryPath, QStringLiteral("binary star catalog cache"), snapshot.catalogBinaryPayload);

    const QString configuredDeepSkyBinaryPath = defaultDeepSkyBinaryCatalogCachePath();
    loadCachePayload(
        configuredDeepSkyBinaryPath, QStringLiteral("binary deep-sky catalog cache"), snapshot.deepSkyBinaryPayload
    );

    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()
        && snapshot.catalogBinaryPayload.isEmpty() && snapshot.deepSkyBinaryPayload.isEmpty()) {
        return std::nullopt;
    }

    if (!snapshot.catalogPayload.isEmpty()) {
        snapshot.sourceLabel =
            settings.value(SkySettingsKeyBuilder::key("catalogSourceLabel"), QString("Saved")).toString();
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        snapshot.deepSkySourceLabel =
            settings.value(SkySettingsKeyBuilder::key("deepSkyCatalogSourceLabel"), QString("Saved deep sky"))
                .toString();
    }
    snapshot.constellationLineRows =
        settings.value(SkySettingsKeyBuilder::key("catalogConstellationLineRefs")).toByteArray();
    snapshot.constellationAnchorGroupRows =
        settings.value(SkySettingsKeyBuilder::key("catalogConstellationAnchorGroups")).toByteArray();
    snapshot.constellationLineSchemaVersion = SkySettingsValueCodecs::readIntSetting(
        settings, SkySettingsKeyBuilder::key("catalogConstellationLineSchemaVersion"), 0
    );
    snapshot.constellationCount = static_cast<std::size_t>(SkySettingsValueCodecs::readULongLongSetting(
        settings, SkySettingsKeyBuilder::key("catalogConstellationCount"), static_cast<qulonglong>(0)
    ));
    snapshot.catalogBinarySchemaVersion =
        SkySettingsValueCodecs::readIntSetting(settings, SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"), 0);
    qCInfo(skygateCatalogCacheLog).noquote()
        << "Catalog cache loaded: starBytes" << snapshot.catalogPayload.size() << "deepSkyBytes"
        << snapshot.deepSkyCatalogPayload.size() << "starBinaryBytes" << snapshot.catalogBinaryPayload.size()
        << "deepSkyBinaryBytes" << snapshot.deepSkyBinaryPayload.size();
    return snapshot;
}

bool SkySettingsStore::saveCatalogCollectionCache(const SkyCatalogCollectionCacheSnapshot& snapshot) const
{
    return m_catalogCollectionStore.save(snapshot);
}

SkyCatalogCollectionCacheLoadResult SkySettingsStore::loadCatalogCollectionCache() const
{
    return m_catalogCollectionStore.load();
}

bool SkySettingsStore::clearCatalogCollectionCache() const
{
    return m_catalogCollectionStore.clear();
}

bool SkySettingsStore::clearCatalogSourceCache(const QString& instanceId) const
{
    return m_catalogCollectionStore.evictSource(instanceId);
}

bool SkySettingsStore::saveEphemerisDataCache(const SkyEphemerisDataCacheSnapshot& snapshot) const
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

SkyEphemerisDataCacheSnapshot SkySettingsStore::loadEphemerisDataCache() const
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

bool SkySettingsStore::clearEphemerisDataCache() const
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
