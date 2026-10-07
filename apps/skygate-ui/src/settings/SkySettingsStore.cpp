#include "SkySettingsStore.hpp"

#include "SkyContextControllerSupport.hpp"
#include "SkySettingsSnapshotCodecs.hpp"
#include "SkySettingsValueCodecs.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
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

// A stored schema hint only means something when it names a schema this build
// knows. A missing or garbled value falls back to "no hint" so detection
// decides, instead of failing the source with a synthetic hint mismatch.
skygate::ephemeris::CatalogSourceType readCatalogSourceSchemaHint(QSettings& settings, const QString& key)
{
    const auto noHint = skygate::ephemeris::CatalogSourceType::Unknown;
    if (!settings.contains(key)) {
        return noHint;
    }

    bool ok = false;
    const int storedValue = settings.value(key).toInt(&ok);
    if (ok) {
        const auto storedType = static_cast<skygate::ephemeris::CatalogSourceType>(storedValue);
        switch (storedType) {
        case skygate::ephemeris::CatalogSourceType::Bundled:
        case skygate::ephemeris::CatalogSourceType::HygCsv:
        case skygate::ephemeris::CatalogSourceType::OpenNgcCsv:
        case skygate::ephemeris::CatalogSourceType::Unknown:
            return storedType;
        }
    }

    qCWarning(skygateSettingsLog).noquote() << "Invalid catalog source schema hint" << key << "value"
                                            << settings.value(key).toString() << "- using fallback: no schema hint";
    return noHint;
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

QString catalogSourceIdDigest(const QString& instanceId)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest.left(16));
}

QString catalogSourceFileStem(const QString& instanceId)
{
    return QStringLiteral("catalog-source-") + catalogSourceIdDigest(instanceId);
}

// Sidecar names embed the generation that produced them, so staging a new
// snapshot never overwrites the files the committed records still reference.
QString catalogSourceGenerationFileStem(const QString& instanceId, const quint64 generation)
{
    return QStringLiteral("catalog-source-g%1-%2").arg(generation).arg(catalogSourceIdDigest(instanceId));
}

QString catalogSourceRawPath(const QString& directory, const QString& instanceId, const quint64 generation)
{
    return QDir(directory).filePath(catalogSourceGenerationFileStem(instanceId, generation) + QStringLiteral(".txt"));
}

QString catalogSourceBinaryPath(const QString& directory, const QString& instanceId, const quint64 generation)
{
    return QDir(directory).filePath(catalogSourceGenerationFileStem(instanceId, generation) + QStringLiteral(".bin"));
}

// Records written before generations were published keep their settings group
// directly under catalogSources; committed generations nest theirs one level
// deeper so staging cannot replace the committed records.
QString catalogSourceSettingsGroup(const QString& instanceId)
{
    return QStringLiteral("catalogSources/") + catalogSourceFileStem(instanceId);
}

QString catalogSourceGenerationSettingsGroup(const quint64 generation, const QString& instanceId)
{
    return QStringLiteral("catalogSources/%1/%2").arg(generation).arg(catalogSourceFileStem(instanceId));
}

QString catalogCollectionGenerationSettingsGroup(const quint64 generation)
{
    return QStringLiteral("catalogSources/%1").arg(generation);
}

QString catalogCollectionRecordsStoredKey()
{
    return QStringLiteral("recordsStored");
}

QString catalogCollectionManifestPath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("catalog-collection.manifest"));
}

// The manifest names the settings namespace and generation that hold the
// committed records. It is published last, after every new payload file and
// record is durable, so its presence alone decides which snapshot is active.
struct CatalogCollectionManifest final {
    int schemaVersion = 0;
    int binarySchemaVersion = 0;
    quint64 generation = 0;
};

bool writeCatalogCollectionManifest(const QString& path, const CatalogCollectionManifest& manifest)
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo targetInfo(path);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to create catalog collection cache directory" << targetDir.absolutePath();
        return false;
    }

    QByteArray content;
    content += QByteArrayLiteral("schemaVersion=") + QByteArray::number(manifest.schemaVersion) + '\n';
    content += QByteArrayLiteral("binarySchemaVersion=") + QByteArray::number(manifest.binarySchemaVersion) + '\n';
    content += QByteArrayLiteral("generation=") + QByteArray::number(manifest.generation) + '\n';

    QSaveFile manifestFile(path);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog collection manifest for writing" << path << manifestFile.errorString();
        return false;
    }

    if (manifestFile.write(content) != content.size()) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Failed to write complete catalog collection manifest" << path;
        manifestFile.cancelWriting();
        return false;
    }

    if (!manifestFile.commit()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to commit catalog collection manifest" << path << manifestFile.errorString();
        return false;
    }
    return true;
}

quint64 highestCatalogCollectionGeneration(
    const QString& directory, QSettings& settings, const std::optional<CatalogCollectionManifest>& manifest
)
{
    quint64 highest = manifest.has_value() ? manifest->generation : 0U;

    // A save interrupted between its staging steps can leave records or
    // sidecar files from a generation the manifest does not name yet. Reusing
    // such a number would overwrite staged data, so the next save skips every
    // generation number it can still observe.
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList recordGroups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : recordGroups) {
        bool ok = false;
        const quint64 generation = group.toULongLong(&ok);
        if (ok && generation > highest) {
            highest = generation;
        }
    }

    if (directory.isEmpty()) {
        return highest;
    }
    const QDir dir(directory);
    const QString prefix = QStringLiteral("catalog-source-g");
    const QStringList sidecarNames =
        dir.entryList(QStringList{prefix + QLatin1Char('*')}, QDir::Files | QDir::Readable);
    for (const QString& sidecarName : sidecarNames) {
        const int separator = sidecarName.indexOf(QLatin1Char('-'), prefix.size());
        if (separator < 0) {
            continue;
        }
        bool ok = false;
        const quint64 generation = sidecarName.mid(prefix.size(), separator - prefix.size()).toULongLong(&ok);
        if (ok && generation > highest) {
            highest = generation;
        }
    }
    return highest;
}

// Generation records live in a numeric group under catalogSources, while the
// flat groups written before generations were published are named after their
// sidecar. A numeric group therefore marks the settings file as
// generation-format even when the manifest is gone.
bool hasCatalogCollectionGenerationGroups(QSettings& settings)
{
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        group.toULongLong(&isGenerationGroup);
        if (isGenerationGroup) {
            return true;
        }
    }
    return false;
}

std::optional<CatalogCollectionManifest> readCatalogCollectionManifest(const QString& path)
{
    if (path.isEmpty()) {
        return std::nullopt;
    }

    QFile manifestFile(path);
    if (!manifestFile.exists()) {
        return std::nullopt;
    }
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog collection manifest" << path << manifestFile.errorString();
        return std::nullopt;
    }

    CatalogCollectionManifest manifest;
    bool hasSchemaVersion = false;
    bool hasBinarySchemaVersion = false;
    bool hasGeneration = false;
    const QList<QByteArray> lines = manifestFile.readAll().split('\n');
    for (const QByteArray& line : lines) {
        const int separator = line.indexOf('=');
        if (separator <= 0) {
            continue;
        }
        const QByteArray key = line.left(separator).trimmed();
        const QByteArray value = line.mid(separator + 1).trimmed();
        bool ok = false;
        if (key == QByteArrayLiteral("schemaVersion")) {
            manifest.schemaVersion = value.toInt(&ok);
            hasSchemaVersion = ok;
        } else if (key == QByteArrayLiteral("binarySchemaVersion")) {
            manifest.binarySchemaVersion = value.toInt(&ok);
            hasBinarySchemaVersion = ok;
        } else if (key == QByteArrayLiteral("generation")) {
            manifest.generation = value.toULongLong(&ok);
            hasGeneration = ok;
        }
    }

    if (!hasSchemaVersion || !hasBinarySchemaVersion || !hasGeneration) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Unreadable catalog collection manifest; no committed collection is available" << path;
        return std::nullopt;
    }
    return manifest;
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
    appendCachePath(cachePaths, catalogCollectionManifestPath(directory));
}

void removeObsoleteCatalogCollectionFiles(const QString& directory, const QStringList& referencedPaths)
{
    if (directory.isEmpty()) {
        return;
    }

    const QDir dir(directory);
    const QStringList entries =
        dir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files | QDir::Readable);
    for (const QString& entryName : entries) {
        const QString path = dir.filePath(entryName);
        if (referencedPaths.contains(path)) {
            continue;
        }
        QFile obsoleteFile(path);
        if (!obsoleteFile.remove()) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Failed to remove obsolete catalog source cache" << path << obsoleteFile.errorString();
        }
    }
}

void removeObsoleteCatalogCollectionRecords(QSettings& settings, const quint64 committedGeneration)
{
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        const quint64 generation = group.toULongLong(&isGenerationGroup);
        if (isGenerationGroup && generation == committedGeneration) {
            continue;
        }
        settings.remove(QStringLiteral("catalogSources/") + group);
    }
}

void saveCatalogSourceRecord(
    QSettings& settings,
    const SkySettingsStore::CatalogSourceCacheRecord& record,
    const QString& directory,
    const quint64 generation
)
{
    settings.beginGroup(catalogSourceGenerationSettingsGroup(generation, record.instanceId));
    settings.setValue(QStringLiteral("instanceId"), record.instanceId);
    settings.setValue(QStringLiteral("descriptorId"), record.descriptorId);
    settings.setValue(QStringLiteral("title"), record.title);
    settings.setValue(QStringLiteral("version"), record.version);
    settings.setValue(QStringLiteral("url"), record.url);
    settings.setValue(QStringLiteral("urls"), record.urls);
    settings.setValue(QStringLiteral("relatedDatasetUrls"), record.relatedDatasetUrls);
    settings.setValue(QStringLiteral("archiveSelector"), record.archiveSelector);
    settings.setValue(QStringLiteral("schemaHint"), static_cast<int>(record.schemaHint));
    settings.setValue(QStringLiteral("attribution"), record.attribution);
    settings.setValue(QStringLiteral("policy"), static_cast<int>(record.policy));
    settings.setValue(QStringLiteral("enabled"), record.enabled);
    settings.setValue(QStringLiteral("bundled"), record.bundled);
    settings.setValue(QStringLiteral("order"), record.order);
    settings.setValue(
        QStringLiteral("payloadPath"),
        record.payload.isEmpty() ? QString() : catalogSourceRawPath(directory, record.instanceId, generation)
    );
    settings.setValue(
        QStringLiteral("binaryPayloadPath"),
        record.binaryPayload.isEmpty() ? QString() : catalogSourceBinaryPath(directory, record.instanceId, generation)
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
    record.url = settings.value(QStringLiteral("url")).toString();
    record.urls = settings.value(QStringLiteral("urls")).toStringList();
    record.relatedDatasetUrls = settings.value(QStringLiteral("relatedDatasetUrls")).toStringList();
    record.archiveSelector = settings.value(QStringLiteral("archiveSelector")).toString();
    record.schemaHint = readCatalogSourceSchemaHint(settings, QStringLiteral("schemaHint"));
    record.attribution = settings.value(QStringLiteral("attribution")).toString();
    record.policy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(readIntSetting(
        settings, QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
    ));
    record.enabled = readBoolSetting(settings, QStringLiteral("enabled"), true);
    record.bundled = readBoolSetting(settings, QStringLiteral("bundled"), false);
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
    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);
    const QString manifestPath = catalogCollectionManifestPath(directory);

    // Every save writes a new generation. The committed generation keeps its
    // records and sidecar payload files until the new generation's manifest is
    // published, so a failure while staging any file leaves the previously
    // committed snapshot intact instead of pairing old metadata with new
    // payloads.
    const std::optional<CatalogCollectionManifest> committed = readCatalogCollectionManifest(manifestPath);
    const quint64 generation = highestCatalogCollectionGeneration(directory, settings, committed) + 1U;

    QStringList stagedPaths;
    const auto discardStagedFiles = [&stagedPaths]() { removeCacheFiles(stagedPaths); };
    for (const CatalogSourceCacheRecord& record : snapshot.sources) {
        if (!record.payload.isEmpty()) {
            const QString rawPath = catalogSourceRawPath(directory, record.instanceId, generation);
            if (!writeCatalogSourceFile(rawPath, record.payload)) {
                discardStagedFiles();
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Failed to stage catalog collection payload; the committed collection remains active:"
                    << record.instanceId;
                return false;
            }
            stagedPaths.push_back(rawPath);
        }
        if (!record.binaryPayload.isEmpty()) {
            const QString binaryPath = catalogSourceBinaryPath(directory, record.instanceId, generation);
            if (!writeCatalogSourceFile(binaryPath, record.binaryPayload)) {
                discardStagedFiles();
                qCWarning(skygateCatalogCacheLog).noquote()
                    << "Failed to stage catalog collection binary payload; the committed collection remains "
                       "active:"
                    << record.instanceId;
                return false;
            }
            stagedPaths.push_back(binaryPath);
        }
    }

    // The new records live in their own settings namespace and are ignored
    // until the manifest below names their generation, so an interrupted or
    // failed publication cannot replace the committed records with metadata
    // that references missing or partial payloads.
    const QString generationGroup = catalogCollectionGenerationSettingsGroup(generation);
    settings.remove(generationGroup);
    settings.setValue(generationGroup + QLatin1Char('/') + catalogCollectionRecordsStoredKey(), true);
    int order = 0;
    for (const CatalogSourceCacheRecord& record : snapshot.sources) {
        CatalogSourceCacheRecord orderedRecord = record;
        orderedRecord.order = order++;
        saveCatalogSourceRecord(settings, orderedRecord, directory, generation);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        settings.remove(generationGroup);
        settings.sync();
        discardStagedFiles();
        qCWarning(skygateCatalogCacheLog)
            << "Failed to store catalog collection records; the committed collection remains active";
        return false;
    }

    // Publishing the manifest commits the generation. It is the durable
    // identifier of the snapshot, so it is written only after every staged
    // payload file and record is already in place. Until it is published the
    // previous manifest keeps the previous generation active, and an empty
    // collection is a committed configuration rather than a cache clear.
    const CatalogCollectionManifest manifest{
        .schemaVersion = snapshot.schemaVersion > 0
                             ? snapshot.schemaVersion
                             : SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion,
        .binarySchemaVersion = snapshot.binarySchemaVersion,
        .generation = generation,
    };
    if (!writeCatalogCollectionManifest(manifestPath, manifest)) {
        settings.remove(generationGroup);
        settings.sync();
        discardStagedFiles();
        qCWarning(skygateCatalogCacheLog)
            << "Failed to publish the catalog collection generation manifest; the committed collection remains active";
        return false;
    }

    // The new generation is durable, so the previous generation and any files
    // abandoned by an interrupted save are unreferenced now. Removing them is
    // best effort: leftovers cannot change which generation is committed.
    removeObsoleteCatalogCollectionFiles(directory, stagedPaths);
    removeObsoleteCatalogCollectionRecords(settings, generation);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog)
            << "Failed to remove obsolete catalog collection records; the committed collection stays active";
    }
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog collection cache saved: sources" << snapshot.sources.size();
    return true;
}

std::optional<SkySettingsStore::CatalogCollectionCacheSnapshot> SkySettingsStore::loadCatalogCollectionCache() const
{
    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);
    const QString recordsStoredKey = catalogCollectionRecordsStoredKey();

    CatalogCollectionCacheSnapshot snapshot;
    QString recordsGroup;
    const std::optional<CatalogCollectionManifest> manifest =
        readCatalogCollectionManifest(catalogCollectionManifestPath(directory));
    if (manifest.has_value()) {
        // The manifest only identifies the committed generation. Its records
        // must also be present in the settings file: a reset settings state
        // must not resurrect a cache directory that happens to still hold a
        // manifest.
        const QString generationGroup = catalogCollectionGenerationSettingsGroup(manifest->generation);
        if (!settings.value(generationGroup + QLatin1Char('/') + recordsStoredKey).toBool()) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest names a generation without stored records"
                << catalogCollectionManifestPath(directory);
            return std::nullopt;
        }
        snapshot.schemaVersion = manifest->schemaVersion;
        snapshot.binarySchemaVersion = manifest->binarySchemaVersion;
        recordsGroup = generationGroup;
    } else {
        const bool generationRecordsExist = hasCatalogCollectionGenerationGroups(settings);
        // Generation-format records or sidecars without a manifest mean the
        // manifest was lost after a generation was committed. Reporting that
        // loss keeps the fallback below diagnosable.
        if (generationRecordsExist || highestCatalogCollectionGeneration(directory, settings, std::nullopt) > 0U) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest is missing while generation-format collection data exists"
                << catalogCollectionManifestPath(directory);
        }
        // Collections written before generations were published keep their
        // version marker and records directly in the settings file.
        if (!settings.contains(SkyContextSettings::key("catalogCollectionVersion"))) {
            return std::nullopt;
        }
        if (generationRecordsExist) {
            // The marker outlived the migration, but the flat records it
            // describes were replaced by generation records. Reading the flat
            // group would report an empty collection with the stale marker's
            // schema version, so no snapshot is returned at all; the committed
            // generation is only reachable again through its manifest.
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Ignoring stale catalog collection version marker: generation-format records exist without a "
                   "committed manifest";
            return std::nullopt;
        }
        snapshot.schemaVersion = readIntSetting(
            settings,
            SkyContextSettings::key("catalogCollectionVersion"),
            SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
        );
        snapshot.binarySchemaVersion =
            readIntSetting(settings, SkyContextSettings::key("catalogBinarySchemaVersion"), 0);
        recordsGroup = QStringLiteral("catalogSources");
    }

    QVector<CatalogSourceCacheRecord> records;
    settings.beginGroup(recordsGroup);
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
    const QString directory = catalogCollectionCacheDirectory(settings);
    const std::optional<CatalogCollectionManifest> manifest =
        readCatalogCollectionManifest(catalogCollectionManifestPath(directory));
    // A published generation keeps its records in its own settings namespace;
    // records written before generations were published keep theirs directly
    // under catalogSources.
    const QString group = manifest.has_value() ? catalogSourceGenerationSettingsGroup(manifest->generation, instanceId)
                                               : catalogSourceSettingsGroup(instanceId);
    settings.beginGroup(group);
    const QString rawPath = settings.value(QStringLiteral("payloadPath")).toString();
    const QString binaryPath = settings.value(QStringLiteral("binaryPayloadPath")).toString();
    settings.endGroup();

    QStringList cachePaths;
    appendCachePath(cachePaths, rawPath);
    appendCachePath(cachePaths, binaryPath);
    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    // Payload eviction keeps the configured record: identity, order, enabled
    // state, descriptor, and parse contract stay durable. The disposable
    // payload references and the owned related dataset they carry are dropped
    // together, so a restart restores the source as configured with an
    // unavailable payload instead of deleting its configuration. Only a later
    // accepted load writes payload references again.
    settings.remove(group + QStringLiteral("/payloadPath"));
    settings.remove(group + QStringLiteral("/binaryPayloadPath"));
    settings.remove(group + QStringLiteral("/constellationLineRows"));
    settings.remove(group + QStringLiteral("/constellationAnchorGroupRows"));
    settings.remove(group + QStringLiteral("/constellationLineSchemaVersion"));
    settings.remove(group + QStringLiteral("/constellationCount"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to evict catalog source payload; the configured record is kept";
        return false;
    }
    if (removedAllCacheFiles) {
        qCInfo(skygateCatalogCacheLog).noquote()
            << "Catalog source payload evicted; configured record kept" << instanceId;
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
