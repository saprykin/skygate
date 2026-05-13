#include "SkySettingsStore.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkySettingsSnapshotCodecs.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
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

QString appSettingsKey(const char* name)
{
    return QStringLiteral("app/%1").arg(QString::fromUtf8(name));
}

QString ephemerisDataCacheKey(const char* name)
{
    return SkyContextSettings::key(QStringLiteral("ephemerisData/%1").arg(QString::fromUtf8(name)));
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

void appendCachePath(QStringList& cachePaths, const QString& path)
{
    if (!path.isEmpty() && !cachePaths.contains(path)) {
        cachePaths.push_back(path);
    }
}

bool removeCacheFiles(const QStringList& cachePaths)
{
    bool removedAllCacheFiles = true;
    for (const QString& cachePath : cachePaths) {
        QFile cacheFile(cachePath);
        if (cacheFile.exists() && !cacheFile.remove()) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Failed to remove catalog cache file" << cachePath << cacheFile.errorString();
            removedAllCacheFiles = false;
        }
    }
    return removedAllCacheFiles;
}

QString readTrimmedStringSetting(QSettings& settings, const QString& key, const QString& fallback = {})
{
    return settings.value(key, fallback).toString().trimmed();
}

}  // namespace

bool SkySettingsStore::saveState(const StateSnapshot& snapshot) const
{
    QSettings settings;
    saveStateSnapshot(settings, snapshot);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateSettingsLog) << "Failed to save SkyGate settings";
        return false;
    }
    return true;
}

std::optional<SkySettingsStore::StateSnapshot> SkySettingsStore::loadState() const
{
    QSettings settings;
    return loadStateSnapshot(settings);
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

bool SkySettingsStore::clearCatalogCache() const
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkyContextSettings::key("catalogCachePath"), SkyContextSettings::defaultCatalogCachePath())
            .toString();
    const QString defaultPath = SkyContextSettings::defaultCatalogCachePath();

    QStringList cachePaths;
    appendCachePath(cachePaths, configuredPath);
    appendCachePath(cachePaths, defaultPath);

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries =
            defaultDir.entryList(QStringList{"catalog-cache*.txt"}, QDir::Files | QDir::Readable);
        for (const QString& entryName : matchingEntries) {
            appendCachePath(cachePaths, defaultDir.filePath(entryName));
        }
    }

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(SkyContextSettings::key("catalogCachePath"));
    settings.remove(SkyContextSettings::key("catalogSourceLabel"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLabelRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogConstellationCount"));
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
    appendCachePath(
        cachePaths,
        settings
            .value(
                SkyContextSettings::key("deepSkyCatalogCachePath"), SkyContextSettings::defaultDeepSkyCatalogCachePath()
            )
            .toString()
    );
    appendCachePath(cachePaths, SkyContextSettings::defaultDeepSkyCatalogCachePath());

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);
    settings.remove(SkyContextSettings::key("deepSkyCatalogCachePath"));
    settings.remove(SkyContextSettings::key("deepSkyCatalogSourceLabel"));
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

bool SkySettingsStore::saveCatalogCache(const CatalogCacheSnapshot& snapshot) const
{
    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString configuredPath =
        settings.value(SkyContextSettings::key("catalogCachePath"), SkyContextSettings::defaultCatalogCachePath())
            .toString();
    const QString configuredDeepSkyPath =
        settings
            .value(
                SkyContextSettings::key("deepSkyCatalogCachePath"), SkyContextSettings::defaultDeepSkyCatalogCachePath()
            )
            .toString();
    const auto writePayload = [](const QString& path, const QByteArray& payload) {
        if (path.isEmpty() || payload.isEmpty()) {
            return true;
        }

        const QFileInfo targetInfo(path);
        QDir targetDir(targetInfo.absolutePath());
        if (!targetDir.mkpath(".")) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Failed to create catalog cache directory" << targetDir.absolutePath();
            return false;
        }

        QSaveFile cacheFile(path);
        if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Failed to open catalog cache for writing" << path << cacheFile.errorString();
            return false;
        }

        const qint64 writtenBytes = cacheFile.write(payload);
        if (writtenBytes != payload.size()) {
            qCWarning(skygateCatalogCacheLog).noquote() << "Failed to write complete catalog cache" << path << "written"
                                                        << writtenBytes << "expected" << payload.size();
            cacheFile.cancelWriting();
            return false;
        }

        if (!cacheFile.commit()) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Failed to commit catalog cache" << path << cacheFile.errorString();
            return false;
        }
        return true;
    };

    if (!writePayload(configuredPath, snapshot.catalogPayload)) {
        return false;
    }
    if (!writePayload(configuredDeepSkyPath, snapshot.deepSkyCatalogPayload)) {
        return false;
    }
    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogCachePath"), configuredPath);
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("deepSkyCatalogCachePath"), configuredDeepSkyPath);
    }
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogSourceLabel"), snapshot.sourceLabel);
        settings.setValue(SkyContextSettings::key("catalogConstellationLineRefs"), snapshot.constellationLineRows);
        settings.setValue(SkyContextSettings::key("catalogConstellationLabelRefs"), snapshot.constellationLabelRows);
        settings.setValue(
            SkyContextSettings::key("catalogConstellationLineSchemaVersion"), snapshot.constellationLineSchemaVersion
        );
        settings.setValue(
            SkyContextSettings::key("catalogConstellationCount"), static_cast<qulonglong>(snapshot.constellationCount)
        );
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("deepSkyCatalogSourceLabel"), snapshot.deepSkySourceLabel);
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

std::optional<SkySettingsStore::CatalogCacheSnapshot> SkySettingsStore::loadCatalogCache() const
{
    QSettings settings;
    const QString configuredPath =
        settings.value(SkyContextSettings::key("catalogCachePath"), SkyContextSettings::defaultCatalogCachePath())
            .toString();

    CatalogCacheSnapshot snapshot;
    QFile cacheFile(configuredPath);
    if (!configuredPath.isEmpty() && cacheFile.exists() && cacheFile.open(QIODevice::ReadOnly)) {
        snapshot.catalogPayload = cacheFile.readAll();
    } else if (!configuredPath.isEmpty() && cacheFile.exists()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open star catalog cache" << configuredPath << cacheFile.errorString();
    }

    const QString configuredDeepSkyPath =
        settings
            .value(
                SkyContextSettings::key("deepSkyCatalogCachePath"), SkyContextSettings::defaultDeepSkyCatalogCachePath()
            )
            .toString();
    QFile deepSkyCacheFile(configuredDeepSkyPath);
    if (!configuredDeepSkyPath.isEmpty() && deepSkyCacheFile.exists() && deepSkyCacheFile.open(QIODevice::ReadOnly)) {
        snapshot.deepSkyCatalogPayload = deepSkyCacheFile.readAll();
    } else if (!configuredDeepSkyPath.isEmpty() && deepSkyCacheFile.exists()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open deep-sky catalog cache" << configuredDeepSkyPath << deepSkyCacheFile.errorString();
    }

    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()) {
        return std::nullopt;
    }

    if (!snapshot.catalogPayload.isEmpty()) {
        snapshot.sourceLabel =
            settings.value(SkyContextSettings::key("catalogSourceLabel"), QString("Saved")).toString();
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        snapshot.deepSkySourceLabel =
            settings.value(SkyContextSettings::key("deepSkyCatalogSourceLabel"), QString("Saved deep sky")).toString();
    }
    snapshot.constellationLineRows =
        settings.value(SkyContextSettings::key("catalogConstellationLineRefs")).toByteArray();
    snapshot.constellationLabelRows =
        settings.value(SkyContextSettings::key("catalogConstellationLabelRefs")).toByteArray();
    snapshot.constellationLineSchemaVersion =
        readIntSetting(settings, SkyContextSettings::key("catalogConstellationLineSchemaVersion"), 0);
    snapshot.constellationCount = static_cast<std::size_t>(
        readULongLongSetting(settings, SkyContextSettings::key("catalogConstellationCount"), static_cast<qulonglong>(0))
    );
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog cache loaded: starBytes" << snapshot.catalogPayload.size()
                                             << "deepSkyBytes" << snapshot.deepSkyCatalogPayload.size();
    return snapshot;
}

bool SkySettingsStore::saveEphemerisDataCache(const EphemerisDataCacheSnapshot& snapshot) const
{
    QSettings settings;
    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    settings.setValue(ephemerisDataCacheKey("installedKernelPath"), snapshot.installedKernelPath);
    settings.setValue(ephemerisDataCacheKey("installedKernelVersion"), snapshot.installedKernelVersion);
    settings.setValue(ephemerisDataCacheKey("installedEarthOrientationPath"), snapshot.installedEarthOrientationPath);
    settings.setValue(
        ephemerisDataCacheKey("installedEarthOrientationVersion"), snapshot.installedEarthOrientationVersion
    );
    settings.setValue(
        ephemerisDataCacheKey("installedLeapSecondTableVersion"), snapshot.installedLeapSecondTableVersion
    );
    settings.setValue(ephemerisDataCacheKey("installedDeltaTDataVersion"), snapshot.installedDeltaTDataVersion);
    settings.setValue(
        ephemerisDataCacheKey("dataRevisionToken"),
        snapshot.dataRevisionToken.trimmed().isEmpty() ? EphemerisDataCacheSnapshot{}.dataRevisionToken
                                                       : snapshot.dataRevisionToken.trimmed()
    );
    settings.setValue(
        ephemerisDataCacheKey("lastUpdateResult"),
        snapshot.lastUpdateResult.trimmed().isEmpty() ? EphemerisDataCacheSnapshot{}.lastUpdateResult
                                                      : snapshot.lastUpdateResult.trimmed()
    );
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateEphemerisDataCacheLog) << "Failed to save ephemeris data cache metadata";
        return false;
    }
    return true;
}

SkySettingsStore::EphemerisDataCacheSnapshot SkySettingsStore::loadEphemerisDataCache() const
{
    QSettings settings;
    EphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelPath = readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedKernelPath"));
    snapshot.installedKernelVersion =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedKernelVersion"));
    snapshot.installedEarthOrientationPath =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedEarthOrientationPath"));
    snapshot.installedEarthOrientationVersion =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedEarthOrientationVersion"));
    snapshot.installedLeapSecondTableVersion =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedLeapSecondTableVersion"));
    snapshot.installedDeltaTDataVersion =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("installedDeltaTDataVersion"));
    snapshot.dataRevisionToken =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("dataRevisionToken"), snapshot.dataRevisionToken);
    if (snapshot.dataRevisionToken.isEmpty()) {
        snapshot.dataRevisionToken = EphemerisDataCacheSnapshot{}.dataRevisionToken;
    }
    snapshot.lastUpdateResult =
        readTrimmedStringSetting(settings, ephemerisDataCacheKey("lastUpdateResult"), snapshot.lastUpdateResult);
    if (snapshot.lastUpdateResult.isEmpty()) {
        snapshot.lastUpdateResult = EphemerisDataCacheSnapshot{}.lastUpdateResult;
    }
    return snapshot;
}

bool SkySettingsStore::clearEphemerisDataCache() const
{
    QSettings settings;
    settings.remove(SkyContextSettings::key("ephemerisData"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateEphemerisDataCacheLog) << "Failed to clear ephemeris data cache metadata";
        return false;
    }
    qCInfo(skygateEphemerisDataCacheLog) << "Ephemeris data cache metadata cleared; bundled fallback is active";
    return true;
}
