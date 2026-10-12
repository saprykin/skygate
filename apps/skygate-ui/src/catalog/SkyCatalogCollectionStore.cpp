#include "SkyCatalogCollectionStore.hpp"
#include "SkyContextSettings.hpp"

#include "catalog/CatalogSchemaRegistry.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")
Q_LOGGING_CATEGORY(skygateSettingsLog, "skygate.settings")

QString defaultCatalogCollectionCachePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(QStringLiteral("catalog-source-cache"));
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
        if (storedType == skygate::ephemeris::CatalogSourceType::Unknown
            || skygate::ephemeris::CatalogSchemaRegistry::find(storedType) != nullptr) {
            return storedType;
        }
    }

    qCWarning(skygateSettingsLog).noquote() << "Invalid catalog source schema hint" << key << "value"
                                            << settings.value(key).toString() << "- using fallback: no schema hint";
    return noHint;
}

QString catalogCollectionCacheDirectory(QSettings& settings)
{
    const QString configuredPath =
        settings.value(SkyContextSettings::key("catalogCollectionCachePath"), defaultCatalogCollectionCachePath())
            .toString();
    return configuredPath.isEmpty() ? defaultCatalogCollectionCachePath() : configuredPath;
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

// Durable proof that a manifest was published at least once, with the last
// committed generation as the recorded recovery point. It is written after the
// manifest commit, so a staged generation that was never published cannot
// masquerade as committed configuration.
QString catalogCollectionCommittedGenerationKey()
{
    return SkyContextSettings::key(QStringLiteral("catalogCollectionCommittedGeneration"));
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

enum class CatalogCollectionManifestState : std::uint8_t {
    Absent,
    Unreadable,
    Readable,
};

struct CatalogCollectionManifestReadResult final {
    CatalogCollectionManifestState state = CatalogCollectionManifestState::Absent;
    CatalogCollectionManifest manifest;
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

quint64
highestCatalogCollectionGeneration(const QString& directory, QSettings& settings, const quint64 committedGeneration)
{
    quint64 highest = committedGeneration;

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

// A non-numeric group under catalogSources is a sidecar-named flat record, so
// its presence proves the pre-generation records are still readable.
bool hasFlatCatalogCollectionRecords(QSettings& settings)
{
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        group.toULongLong(&isGenerationGroup);
        if (!isGenerationGroup) {
            return true;
        }
    }
    return false;
}

CatalogCollectionManifestReadResult readCatalogCollectionManifest(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QFile manifestFile(path);
    if (!manifestFile.exists()) {
        return {};
    }
    // Something that is not a regular file cannot hold a committed manifest;
    // an occupied path like a directory is reported by the save path instead.
    if (!QFileInfo(path).isFile()) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Catalog collection manifest path is not a regular file" << path;
        return {};
    }
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog collection manifest" << path << manifestFile.errorString();
        return {.state = CatalogCollectionManifestState::Unreadable};
    }

    CatalogCollectionManifestReadResult result;
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
            result.manifest.schemaVersion = value.toInt(&ok);
            hasSchemaVersion = ok;
        } else if (key == QByteArrayLiteral("binarySchemaVersion")) {
            result.manifest.binarySchemaVersion = value.toInt(&ok);
            hasBinarySchemaVersion = ok;
        } else if (key == QByteArrayLiteral("generation")) {
            result.manifest.generation = value.toULongLong(&ok);
            hasGeneration = ok;
        }
    }

    if (!hasSchemaVersion || !hasBinarySchemaVersion || !hasGeneration) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Unreadable catalog collection manifest; the committed collection cannot be loaded" << path;
        return {.state = CatalogCollectionManifestState::Unreadable};
    }
    result.state = CatalogCollectionManifestState::Readable;
    return result;
}

// A readable manifest proves that its generation was committed. Recording that
// once keeps the proof available if the manifest is later lost, and it keeps
// the recovery point at the generation the manifest named rather than at a
// staged leftover with a higher number.
void rememberCommittedCatalogCollectionGeneration(QSettings& settings, const quint64 generation)
{
    const QString key = catalogCollectionCommittedGenerationKey();
    bool hasGeneration = false;
    const quint64 recorded = settings.value(key).toULongLong(&hasGeneration);
    if (hasGeneration && recorded == generation) {
        return;
    }

    settings.setValue(key, static_cast<qulonglong>(generation));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog) << "Failed to record the committed catalog collection generation";
    }
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

// A referenced sidecar is expected to exist once its path is stored. A missing
// or unreadable file is corrupt committed data, not a deliberately evicted
// payload, which keeps no reference at all.
struct CatalogSourcePayloadReadResult final {
    QByteArray payload;
    bool unreadable = false;
};

CatalogSourcePayloadReadResult readCatalogSourceFile(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QFile cacheFile(path);
    if (!cacheFile.exists()) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Referenced catalog source payload is missing" << path;
        return {.unreadable = true};
    }
    if (!cacheFile.open(QIODevice::ReadOnly)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog source cache" << path << cacheFile.errorString();
        return {.unreadable = true};
    }
    return {.payload = cacheFile.readAll()};
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
    QSettings& settings, const SkyCatalogSourceCacheRecord& record, const QString& directory, const quint64 generation
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

SkyCatalogSourceCacheRecord
loadCatalogSourceRecord(QSettings& settings, const QString& group, QStringList& unreadablePayloadInstanceIds)
{
    SkyCatalogSourceCacheRecord record;
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
    record.policy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(SkySettingsValueCodecs::readIntSetting(
        settings, QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
    ));
    record.enabled = SkySettingsValueCodecs::readBoolSetting(settings, QStringLiteral("enabled"), true);
    record.bundled = SkySettingsValueCodecs::readBoolSetting(settings, QStringLiteral("bundled"), false);
    record.order = SkySettingsValueCodecs::readIntSetting(settings, QStringLiteral("order"), 0);
    record.constellationLineRows = settings.value(QStringLiteral("constellationLineRows")).toByteArray();
    record.constellationAnchorGroupRows = settings.value(QStringLiteral("constellationAnchorGroupRows")).toByteArray();
    record.constellationLineSchemaVersion =
        SkySettingsValueCodecs::readIntSetting(settings, QStringLiteral("constellationLineSchemaVersion"), 0);
    record.constellationCount = static_cast<std::size_t>(SkySettingsValueCodecs::readULongLongSetting(
        settings, QStringLiteral("constellationCount"), static_cast<qulonglong>(0)
    ));
    const QString rawPath = settings.value(QStringLiteral("payloadPath")).toString();
    const QString binaryPath = settings.value(QStringLiteral("binaryPayloadPath")).toString();
    settings.endGroup();

    const CatalogSourcePayloadReadResult rawPayload = readCatalogSourceFile(rawPath);
    const CatalogSourcePayloadReadResult binaryPayload = readCatalogSourceFile(binaryPath);
    record.payload = rawPayload.payload;
    record.binaryPayload = binaryPayload.payload;
    if ((rawPayload.unreadable || binaryPayload.unreadable) && !record.instanceId.isEmpty()
        && !unreadablePayloadInstanceIds.contains(record.instanceId)) {
        unreadablePayloadInstanceIds.push_back(record.instanceId);
    }
    return record;
}

}  // namespace

bool SkyCatalogCollectionStore::save(const SkyCatalogCollectionCacheSnapshot& snapshot) const
{
    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);
    const QString manifestPath = catalogCollectionManifestPath(directory);

    // Every save writes a new generation. The committed generation keeps its
    // records and sidecar payload files until the new generation's manifest is
    // published, so a failure while staging any file leaves the previously
    // committed snapshot intact instead of pairing old metadata with new
    // payloads.
    const CatalogCollectionManifestReadResult committedManifest = readCatalogCollectionManifest(manifestPath);
    const quint64 committedGeneration = committedManifest.state == CatalogCollectionManifestState::Readable
                                            ? committedManifest.manifest.generation
                                            : 0U;
    const quint64 generation = highestCatalogCollectionGeneration(directory, settings, committedGeneration) + 1U;

    QStringList stagedPaths;
    const auto discardStagedFiles = [&stagedPaths]() { removeCacheFiles(stagedPaths); };
    for (const SkyCatalogSourceCacheRecord& record : snapshot.sources) {
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
    for (const SkyCatalogSourceCacheRecord& record : snapshot.sources) {
        SkyCatalogSourceCacheRecord orderedRecord = record;
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
        .schemaVersion = snapshot.schemaVersion > 0 ? snapshot.schemaVersion : kCollectionSchemaVersion,
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

    // The manifest is durable, so the commit record may now name its
    // generation. A staged generation that never reached this point leaves the
    // record pointing at the previous committed generation.
    settings.setValue(catalogCollectionCommittedGenerationKey(), static_cast<qulonglong>(generation));

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

SkyCatalogCollectionCacheLoadResult SkyCatalogCollectionStore::load() const
{
    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);
    const QString manifestPath = catalogCollectionManifestPath(directory);
    const QString recordsStoredKey = catalogCollectionRecordsStoredKey();

    SkyCatalogCollectionCacheLoadResult result;
    QString recordsGroup;
    const CatalogCollectionManifestReadResult manifest = readCatalogCollectionManifest(manifestPath);
    if (manifest.state == CatalogCollectionManifestState::Readable) {
        // The manifest only identifies the committed generation. Its records
        // must also be present in the settings file: a reset settings state
        // must not resurrect a cache directory that happens to still hold a
        // manifest.
        const QString generationGroup = catalogCollectionGenerationSettingsGroup(manifest.manifest.generation);
        if (!settings.value(generationGroup + QLatin1Char('/') + recordsStoredKey).toBool()) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest names a generation without stored records" << manifestPath;
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.failure = SkyCatalogCollectionCacheLoadResult::Failure::CommittedGenerationMissing;
            result.diagnostic =
                QStringLiteral(
                    "The committed catalog collection manifest names generation %1, whose stored records "
                    "are missing"
                )
                    .arg(manifest.manifest.generation);
            return result;
        }
        result.snapshot.schemaVersion = manifest.manifest.schemaVersion;
        result.snapshot.binarySchemaVersion = manifest.manifest.binarySchemaVersion;
        recordsGroup = generationGroup;
        // A readable manifest is durable commit knowledge even when an earlier
        // build published it, so it is recorded here for later manifest loss
        // and keeps the recovery point at the generation it names.
        rememberCommittedCatalogCollectionGeneration(settings, manifest.manifest.generation);
    } else {
        if (manifest.state == CatalogCollectionManifestState::Unreadable) {
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.failure = SkyCatalogCollectionCacheLoadResult::Failure::ManifestUnreadable;
            result.diagnostic =
                QStringLiteral("The committed catalog collection manifest is unreadable: %1").arg(manifestPath);
            bool hasCommittedGeneration = false;
            const quint64 committedGeneration =
                settings.value(catalogCollectionCommittedGenerationKey()).toULongLong(&hasCommittedGeneration);
            if (hasCommittedGeneration && committedGeneration > 0U) {
                result.diagnostic +=
                    QStringLiteral("; generation %1 was recorded as committed").arg(committedGeneration);
            }
            return result;
        }

        bool hasCommittedGeneration = false;
        const quint64 committedGeneration =
            settings.value(catalogCollectionCommittedGenerationKey()).toULongLong(&hasCommittedGeneration);
        const bool generationDataExists = hasCatalogCollectionGenerationGroups(settings)
                                          || highestCatalogCollectionGeneration(directory, settings, 0U) > 0U;
        // Generation-format records or sidecars without a manifest are
        // reported so the outcome below stays diagnosable; only the commit
        // record proves that a manifest was actually published.
        if (generationDataExists) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest is missing while generation-format collection data exists"
                << manifestPath;
        }
        if (hasCommittedGeneration && committedGeneration > 0U) {
            qCWarning(skygateCatalogCacheLog).noquote() << "Catalog collection manifest is missing while generation"
                                                        << committedGeneration << "was recorded as committed";
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.failure = SkyCatalogCollectionCacheLoadResult::Failure::CommittedManifestMissing;
            result.diagnostic =
                QStringLiteral(
                    "The committed catalog collection manifest is missing while generation %1 was recorded as "
                    "committed"
                )
                    .arg(committedGeneration);
            return result;
        }
        // Collections written before generations were published keep their
        // version marker and records directly in the settings file.
        if (!settings.contains(SkyContextSettings::key("catalogCollectionVersion"))) {
            // Generation groups or sidecars left behind by an interrupted first
            // save are not proof that a collection was ever committed, so the
            // outcome stays first use while the ignored data remains
            // diagnosable.
            if (generationDataExists) {
                result.diagnostic =
                    QStringLiteral("Ignoring generation-format catalog collection data without a committed manifest");
            }
            return result;
        }
        const bool stagedGenerationGroupsExist = hasCatalogCollectionGenerationGroups(settings);
        if (stagedGenerationGroupsExist && hasFlatCatalogCollectionRecords(settings)) {
            // An interrupted first migration staged generation records without
            // publishing a manifest, but the flat records it would replace are
            // still intact. Those records already describe a committed
            // collection, so they stay the readable configuration while the
            // staged generation data is ignored and reported.
            result.diagnostic = QStringLiteral(
                "Ignoring generation-format catalog collection data without a committed manifest; the flat "
                "records remain readable"
            );
        } else if (stagedGenerationGroupsExist) {
            // The marker outlived the migration, but the flat records it
            // describes were replaced by generation records. Reading the flat
            // group would report an empty collection with the stale marker's
            // schema version, so the committed configuration is unusable
            // instead.
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Ignoring stale catalog collection version marker: generation-format records exist without a "
                   "committed manifest";
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.failure = SkyCatalogCollectionCacheLoadResult::Failure::LegacyRecordsReplaced;
            result.diagnostic = QStringLiteral(
                "The catalog collection version marker outlived its records, which generation-format data replaced"
            );
            return result;
        }
        result.snapshot.schemaVersion = SkySettingsValueCodecs::readIntSetting(
            settings, SkyContextSettings::key("catalogCollectionVersion"), kCollectionSchemaVersion
        );
        result.snapshot.binarySchemaVersion =
            SkySettingsValueCodecs::readIntSetting(settings, SkyContextSettings::key("catalogBinarySchemaVersion"), 0);
        recordsGroup = QStringLiteral("catalogSources");
    }

    QVector<SkyCatalogSourceCacheRecord> records;
    settings.beginGroup(recordsGroup);
    const QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        group.toULongLong(&isGenerationGroup);
        if (isGenerationGroup) {
            // Staged generation records live in numeric groups; only the flat
            // sidecar-named groups under the current group are source records.
            continue;
        }
        SkyCatalogSourceCacheRecord record =
            loadCatalogSourceRecord(settings, group, result.unreadablePayloadInstanceIds);
        if (!record.instanceId.isEmpty()) {
            records.push_back(std::move(record));
        }
    }
    settings.endGroup();

    std::stable_sort(
        records.begin(),
        records.end(),
        [](const SkyCatalogSourceCacheRecord& lhs, const SkyCatalogSourceCacheRecord& rhs) {
            return lhs.order < rhs.order;
        }
    );
    result.snapshot.sources = std::move(records);
    result.state = SkyCatalogCollectionCacheLoadResult::State::Loaded;
    qCInfo(skygateCatalogCacheLog).noquote()
        << "Catalog collection cache loaded: sources" << result.snapshot.sources.size();
    return result;
}

bool SkyCatalogCollectionStore::clear() const
{
    QSettings settings;
    const QString configuredDirectory = catalogCollectionCacheDirectory(settings);
    const QString defaultDirectory = defaultCatalogCollectionCachePath();

    QStringList cachePaths;
    appendCatalogCollectionDirFiles(cachePaths, configuredDirectory);
    appendCatalogCollectionDirFiles(cachePaths, defaultDirectory);
    const bool removedAllCacheFiles = removeCacheFiles(cachePaths);

    settings.remove(QStringLiteral("catalogSources"));
    settings.remove(SkyContextSettings::key("catalogCollectionVersion"));
    settings.remove(SkyContextSettings::key("catalogBinarySchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogCollectionCachePath"));
    settings.remove(catalogCollectionCommittedGenerationKey());
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

bool SkyCatalogCollectionStore::evictSource(const QString& instanceId) const
{
    if (instanceId.isEmpty()) {
        return false;
    }

    QSettings settings;
    const QString directory = catalogCollectionCacheDirectory(settings);
    const CatalogCollectionManifestReadResult manifest =
        readCatalogCollectionManifest(catalogCollectionManifestPath(directory));
    // A published generation keeps its records in its own settings namespace;
    // records written before generations were published keep theirs directly
    // under catalogSources.
    const QString group = manifest.state == CatalogCollectionManifestState::Readable
                              ? catalogSourceGenerationSettingsGroup(manifest.manifest.generation, instanceId)
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

}  // namespace skygate::ui::internal
