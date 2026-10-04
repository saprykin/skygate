#include "SkySettingsStore.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkySettingsSnapshotCodecs.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSettings>

#include <algorithm>
#include <optional>
#include <utility>

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

QString catalogCollectionCacheDirectory(QSettings& settings)
{
    const QString configuredPath = settings
                                       .value(
                                           SkyContextSettings::key("catalogCollectionCachePath"),
                                           SkyContextSettings::defaultCatalogCollectionCachePath()
                                       )
                                       .toString();
    return configuredPath.isEmpty() ? SkyContextSettings::defaultCatalogCollectionCachePath() : configuredPath;
}

QString catalogSourceFileStem(const QString& instanceId)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("catalog-source-") + QString::fromLatin1(digest.left(16));
}

QString catalogSourceRawPath(const QString& directory, const QString& instanceId)
{
    return QDir(directory).filePath(catalogSourceFileStem(instanceId) + QStringLiteral(".txt"));
}

QString catalogSourceBinaryPath(const QString& directory, const QString& instanceId)
{
    return QDir(directory).filePath(catalogSourceFileStem(instanceId) + QStringLiteral(".bin"));
}

QString catalogSourceSettingsGroup(const QString& instanceId)
{
    return QStringLiteral("catalogSources/") + catalogSourceFileStem(instanceId);
}

bool writeCatalogSourceFile(const QString& path, const QByteArray& payload)
{
    if (path.isEmpty() || payload.isEmpty()) {
        return true;
    }

    const QFileInfo targetInfo(path);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to create catalog collection cache directory" << targetDir.absolutePath();
        return false;
    }

    QSaveFile cacheFile(path);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog source cache for writing" << path << cacheFile.errorString();
        return false;
    }

    const qint64 writtenBytes = cacheFile.write(payload);
    if (writtenBytes != payload.size()) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Failed to write complete catalog source cache" << path
                                                    << "written" << writtenBytes << "expected" << payload.size();
        cacheFile.cancelWriting();
        return false;
    }

    if (!cacheFile.commit()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to commit catalog source cache" << path << cacheFile.errorString();
        return false;
    }
    return true;
}

QByteArray readCatalogSourceFile(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QFile cacheFile(path);
    if (!cacheFile.exists()) {
        return {};
    }
    if (!cacheFile.open(QIODevice::ReadOnly)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog source cache" << path << cacheFile.errorString();
        return {};
    }
    return cacheFile.readAll();
}

void appendCatalogCollectionDirFiles(QStringList& cachePaths, const QString& directory)
{
    if (directory.isEmpty()) {
        return;
    }

    const QDir dir(directory);
    const QStringList entries =
        dir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files | QDir::Readable);
    for (const QString& entryName : entries) {
        appendCachePath(cachePaths, dir.filePath(entryName));
    }
}

void saveCatalogSourceRecord(
    QSettings& settings, const SkySettingsStore::CatalogSourceCacheRecord& record, const QString& directory
)
{
    settings.beginGroup(catalogSourceSettingsGroup(record.instanceId));
    settings.setValue(QStringLiteral("instanceId"), record.instanceId);
    settings.setValue(QStringLiteral("descriptorId"), record.descriptorId);
    settings.setValue(QStringLiteral("title"), record.title);
    settings.setValue(QStringLiteral("version"), record.version);
    settings.setValue(QStringLiteral("urls"), record.urls);
    settings.setValue(QStringLiteral("relatedDatasetUrls"), record.relatedDatasetUrls);
    settings.setValue(QStringLiteral("archiveSelector"), record.archiveSelector);
    settings.setValue(QStringLiteral("policy"), static_cast<int>(record.policy));
    settings.setValue(QStringLiteral("enabled"), record.enabled);
    settings.setValue(QStringLiteral("order"), record.order);
    settings.setValue(
        QStringLiteral("payloadPath"),
        record.payload.isEmpty() ? QString() : catalogSourceRawPath(directory, record.instanceId)
    );
    settings.setValue(
        QStringLiteral("binaryPayloadPath"),
        record.binaryPayload.isEmpty() ? QString() : catalogSourceBinaryPath(directory, record.instanceId)
    );
    settings.setValue(QStringLiteral("constellationLineRows"), record.constellationLineRows);
    settings.setValue(QStringLiteral("constellationAnchorGroupRows"), record.constellationAnchorGroupRows);
    settings.setValue(QStringLiteral("constellationLineSchemaVersion"), record.constellationLineSchemaVersion);
    settings.setValue(QStringLiteral("constellationCount"), static_cast<qulonglong>(record.constellationCount));
    settings.endGroup();
}

SkySettingsStore::CatalogSourceCacheRecord loadCatalogSourceRecord(QSettings& settings, const QString& group)
{
    SkySettingsStore::CatalogSourceCacheRecord record;
    settings.beginGroup(group);
    record.instanceId = settings.value(QStringLiteral("instanceId")).toString();
    record.descriptorId = settings.value(QStringLiteral("descriptorId")).toString();
    record.title = settings.value(QStringLiteral("title")).toString();
    record.version = settings.value(QStringLiteral("version")).toString();
    record.urls = settings.value(QStringLiteral("urls")).toStringList();
    record.relatedDatasetUrls = settings.value(QStringLiteral("relatedDatasetUrls")).toStringList();
    record.archiveSelector = settings.value(QStringLiteral("archiveSelector")).toString();
    record.policy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(readIntSetting(
        settings, QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
    ));
    record.enabled = readBoolSetting(settings, QStringLiteral("enabled"), true);
    record.order = readIntSetting(settings, QStringLiteral("order"), 0);
    record.constellationLineRows = settings.value(QStringLiteral("constellationLineRows")).toByteArray();
    record.constellationAnchorGroupRows = settings.value(QStringLiteral("constellationAnchorGroupRows")).toByteArray();
    record.constellationLineSchemaVersion =
        readIntSetting(settings, QStringLiteral("constellationLineSchemaVersion"), 0);
    record.constellationCount = static_cast<std::size_t>(
        readULongLongSetting(settings, QStringLiteral("constellationCount"), static_cast<qulonglong>(0))
    );
    const QString rawPath = settings.value(QStringLiteral("payloadPath")).toString();
    const QString binaryPath = settings.value(QStringLiteral("binaryPayloadPath")).toString();
    settings.endGroup();

    record.payload = readCatalogSourceFile(rawPath);
    record.binaryPayload = readCatalogSourceFile(binaryPath);
    return record;
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
    appendCachePath(cachePaths, SkyContextSettings::defaultCatalogBinaryCachePath());

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries =
            defaultDir.entryList(QStringList{"catalog-cache*"}, QDir::Files | QDir::Readable);
        for (const QString& entryName : matchingEntries) {
            appendCachePath(cachePaths, defaultDir.filePath(entryName));
        }
    }

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(SkyContextSettings::key("catalogCachePath"));
    settings.remove(SkyContextSettings::key("catalogBinaryCachePath"));
    settings.remove(SkyContextSettings::key("catalogBinarySchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogSourceLabel"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationAnchorGroups"));
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
    appendCachePath(cachePaths, SkyContextSettings::defaultDeepSkyBinaryCatalogCachePath());

    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);
    settings.remove(SkyContextSettings::key("deepSkyCatalogCachePath"));
    settings.remove(SkyContextSettings::key("deepSkyBinaryCatalogCachePath"));
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
    const QString configuredBinaryPath = SkyContextSettings::defaultCatalogBinaryCachePath();
    const QString configuredDeepSkyBinaryPath = SkyContextSettings::defaultDeepSkyBinaryCatalogCachePath();
    if (!writePayload(configuredBinaryPath, snapshot.catalogBinaryPayload)) {
        return false;
    }
    if (!writePayload(configuredDeepSkyBinaryPath, snapshot.deepSkyBinaryPayload)) {
        return false;
    }
    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogCachePath"), configuredPath);
    }
    if (!snapshot.deepSkyCatalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("deepSkyCatalogCachePath"), configuredDeepSkyPath);
    }
    if (!snapshot.catalogBinaryPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogBinaryCachePath"), configuredBinaryPath);
        settings.setValue(SkyContextSettings::key("catalogBinarySchemaVersion"), snapshot.catalogBinarySchemaVersion);
    }
    if (!snapshot.deepSkyBinaryPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("deepSkyBinaryCatalogCachePath"), configuredDeepSkyBinaryPath);
    }
    if (!snapshot.catalogPayload.isEmpty()) {
        settings.setValue(SkyContextSettings::key("catalogSourceLabel"), snapshot.sourceLabel);
        settings.setValue(SkyContextSettings::key("catalogConstellationLineRefs"), snapshot.constellationLineRows);
        settings.setValue(
            SkyContextSettings::key("catalogConstellationAnchorGroups"), snapshot.constellationAnchorGroupRows
        );
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

    const QString configuredBinaryPath = SkyContextSettings::defaultCatalogBinaryCachePath();
    QFile binaryCacheFile(configuredBinaryPath);
    if (!configuredBinaryPath.isEmpty() && binaryCacheFile.exists() && binaryCacheFile.open(QIODevice::ReadOnly)) {
        snapshot.catalogBinaryPayload = binaryCacheFile.readAll();
    } else if (!configuredBinaryPath.isEmpty() && binaryCacheFile.exists()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open binary star catalog cache" << configuredBinaryPath << binaryCacheFile.errorString();
    }

    const QString configuredDeepSkyBinaryPath = SkyContextSettings::defaultDeepSkyBinaryCatalogCachePath();
    QFile deepSkyBinaryCacheFile(configuredDeepSkyBinaryPath);
    if (!configuredDeepSkyBinaryPath.isEmpty() && deepSkyBinaryCacheFile.exists()
        && deepSkyBinaryCacheFile.open(QIODevice::ReadOnly)) {
        snapshot.deepSkyBinaryPayload = deepSkyBinaryCacheFile.readAll();
    } else if (!configuredDeepSkyBinaryPath.isEmpty() && deepSkyBinaryCacheFile.exists()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open binary deep-sky catalog cache" << configuredDeepSkyBinaryPath
            << deepSkyBinaryCacheFile.errorString();
    }

    if (snapshot.catalogPayload.isEmpty() && snapshot.deepSkyCatalogPayload.isEmpty()
        && snapshot.catalogBinaryPayload.isEmpty() && snapshot.deepSkyBinaryPayload.isEmpty()) {
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
    snapshot.constellationAnchorGroupRows =
        settings.value(SkyContextSettings::key("catalogConstellationAnchorGroups")).toByteArray();
    snapshot.constellationLineSchemaVersion =
        readIntSetting(settings, SkyContextSettings::key("catalogConstellationLineSchemaVersion"), 0);
    snapshot.constellationCount = static_cast<std::size_t>(
        readULongLongSetting(settings, SkyContextSettings::key("catalogConstellationCount"), static_cast<qulonglong>(0))
    );
    snapshot.catalogBinarySchemaVersion =
        readIntSetting(settings, SkyContextSettings::key("catalogBinarySchemaVersion"), 0);
    qCInfo(skygateCatalogCacheLog).noquote()
        << "Catalog cache loaded: starBytes" << snapshot.catalogPayload.size() << "deepSkyBytes"
        << snapshot.deepSkyCatalogPayload.size() << "starBinaryBytes" << snapshot.catalogBinaryPayload.size()
        << "deepSkyBinaryBytes" << snapshot.deepSkyBinaryPayload.size();
    return snapshot;
}

bool SkySettingsStore::saveCatalogCollectionCache(const CatalogCollectionCacheSnapshot& snapshot) const
{
    if (snapshot.sources.isEmpty()) {
        return clearCatalogCollectionCache();
    }

    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);

    // Write sidecar payload files before touching QSettings so a failed write
    // leaves the previously persisted collection recoverable.
    for (const CatalogSourceCacheRecord& record : snapshot.sources) {
        if (!record.payload.isEmpty()
            && !writeCatalogSourceFile(catalogSourceRawPath(directory, record.instanceId), record.payload)) {
            return false;
        }
        if (!record.binaryPayload.isEmpty()
            && !writeCatalogSourceFile(catalogSourceBinaryPath(directory, record.instanceId), record.binaryPayload)) {
            return false;
        }
    }

    settings.remove(QStringLiteral("catalogSources"));
    settings.setValue(
        SkyContextSettings::key("catalogCollectionVersion"),
        snapshot.schemaVersion > 0 ? snapshot.schemaVersion
                                   : SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    settings.setValue(SkyContextSettings::key("catalogBinarySchemaVersion"), snapshot.binarySchemaVersion);

    int order = 0;
    for (const CatalogSourceCacheRecord& record : snapshot.sources) {
        CatalogSourceCacheRecord orderedRecord = record;
        orderedRecord.order = order++;
        saveCatalogSourceRecord(settings, orderedRecord, directory);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to save catalog collection cache settings";
        return false;
    }
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog collection cache saved: sources" << snapshot.sources.size();
    return true;
}

std::optional<SkySettingsStore::CatalogCollectionCacheSnapshot> SkySettingsStore::loadCatalogCollectionCache() const
{
    QSettings settings;
    if (!settings.contains(SkyContextSettings::key("catalogCollectionVersion"))) {
        return std::nullopt;
    }

    CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = readIntSetting(
        settings,
        SkyContextSettings::key("catalogCollectionVersion"),
        SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    snapshot.binarySchemaVersion = readIntSetting(settings, SkyContextSettings::key("catalogBinarySchemaVersion"), 0);

    QVector<CatalogSourceCacheRecord> records;
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        CatalogSourceCacheRecord record = loadCatalogSourceRecord(settings, group);
        if (!record.instanceId.isEmpty()) {
            records.push_back(std::move(record));
        }
    }
    settings.endGroup();

    std::stable_sort(
        records.begin(), records.end(), [](const CatalogSourceCacheRecord& lhs, const CatalogSourceCacheRecord& rhs) {
            return lhs.order < rhs.order;
        }
    );
    snapshot.sources = std::move(records);
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog collection cache loaded: sources" << snapshot.sources.size();
    return snapshot;
}

bool SkySettingsStore::clearCatalogCollectionCache() const
{
    QSettings settings;
    const QString configuredDirectory = catalogCollectionCacheDirectory(settings);
    const QString defaultDirectory = SkyContextSettings::defaultCatalogCollectionCachePath();

    QStringList cachePaths;
    appendCatalogCollectionDirFiles(cachePaths, configuredDirectory);
    appendCatalogCollectionDirFiles(cachePaths, defaultDirectory);
    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(QStringLiteral("catalogSources"));
    settings.remove(SkyContextSettings::key("catalogCollectionVersion"));
    settings.remove(SkyContextSettings::key("catalogBinarySchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogCollectionCachePath"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear catalog collection cache settings";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote() << "Catalog collection cache cleared: files" << cachePaths.size();
    }
    return removedAllCacheFiles;
}

bool SkySettingsStore::clearCatalogSourceCache(const QString& instanceId) const
{
    if (instanceId.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString group = catalogSourceSettingsGroup(instanceId);
    settings.beginGroup(group);
    const QString rawPath = settings.value(QStringLiteral("payloadPath")).toString();
    const QString binaryPath = settings.value(QStringLiteral("binaryPayloadPath")).toString();
    settings.endGroup();

    QStringList cachePaths;
    appendCachePath(cachePaths, rawPath);
    appendCachePath(cachePaths, binaryPath);
    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(group);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to clear catalog source cache settings";
        return false;
    }
    return removedAllCacheFiles;
}

bool SkySettingsStore::saveEphemerisDataCache(const EphemerisDataCacheSnapshot& snapshot) const
{
    QSettings settings;
    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    settings.setValue(ephemerisDataCacheKey("installedKernelAssetId"), snapshot.installedKernelAssetId);
    settings.setValue(ephemerisDataCacheKey("installedKernelProfileId"), snapshot.installedKernelProfileId);
    settings.setValue(ephemerisDataCacheKey("installedKernelPath"), snapshot.installedKernelPath);
    settings.setValue(ephemerisDataCacheKey("installedKernelVersion"), snapshot.installedKernelVersion);
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
    snapshot.installedKernelAssetId = settings.value(ephemerisDataCacheKey("installedKernelAssetId")).toString();
    snapshot.installedKernelProfileId = settings.value(ephemerisDataCacheKey("installedKernelProfileId")).toString();
    snapshot.installedKernelPath = settings.value(ephemerisDataCacheKey("installedKernelPath")).toString();
    snapshot.installedKernelVersion = settings.value(ephemerisDataCacheKey("installedKernelVersion")).toString();
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
