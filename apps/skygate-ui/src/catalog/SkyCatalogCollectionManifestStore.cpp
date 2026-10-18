#include "SkyCatalogCollectionManifestStore.hpp"
#include "SkyCatalogSourceCachePaths.hpp"
#include "SkyCatalogSourceRecordCodec.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkySettingsKeyBuilder.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QLoggingCategory>
#include <QSettings>

#include <optional>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")

QString manifestFileName()
{
    return QStringLiteral("catalog-collection.manifest");
}

QString committedGenerationKey()
{
    return SkySettingsKeyBuilder::key(QStringLiteral("catalogCollectionCommittedGeneration"));
}

QString recordsStoredKey(const quint64 generation)
{
    return SkyCatalogCollectionManifestStore::generationGroup(generation) + QLatin1Char('/')
           + QStringLiteral("recordsStored");
}

quint64 highestGeneration(const QString& directory, QSettings& settings, const quint64 committedGeneration)
{
    quint64 highest = committedGeneration;

    settings.beginGroup(SkyCatalogSourceRecordCodec::sourcesGroup());
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
    const QStringList sidecarNames =
        dir.entryList(QStringList{SkyCatalogSourceCachePaths::sidecarEntryPattern()}, QDir::Files | QDir::Readable);
    for (const QString& sidecarName : sidecarNames) {
        const std::optional<quint64> generation = SkyCatalogSourceCachePaths::generationFromSidecarName(sidecarName);
        if (generation.has_value() && generation.value() > highest) {
            highest = generation.value();
        }
    }
    return highest;
}

void removeObsoleteFiles(const QString& directory, const QStringList& referencedPaths)
{
    if (directory.isEmpty()) {
        return;
    }

    const QDir dir(directory);
    const QStringList entries =
        dir.entryList(QStringList{SkyCatalogSourceCachePaths::sidecarEntryPattern()}, QDir::Files | QDir::Readable);
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

void removeObsoleteRecords(QSettings& settings, const quint64 committedGeneration)
{
    settings.beginGroup(SkyCatalogSourceRecordCodec::sourcesGroup());
    const QStringList groups = settings.childGroups();
    settings.endGroup();
    for (const QString& group : groups) {
        bool isGenerationGroup = false;
        const quint64 generation = group.toULongLong(&isGenerationGroup);
        if (isGenerationGroup && generation == committedGeneration) {
            continue;
        }
        settings.remove(SkyCatalogSourceRecordCodec::sourcesGroup() + QLatin1Char('/') + group);
    }
}

}  // namespace

QString SkyCatalogCollectionManifestStore::path(const QString& directory)
{
    return QDir(directory).filePath(manifestFileName());
}

SkyCatalogCollectionManifestStore::ReadResult SkyCatalogCollectionManifestStore::read(const QString& directory)
{
    const QString manifestPath = path(directory);
    if (manifestPath.isEmpty()) {
        return {};
    }

    QFile manifestFile(manifestPath);
    if (!manifestFile.exists()) {
        return {};
    }
    // Something that is not a regular file cannot hold a committed manifest;
    // an occupied path like a directory is reported by the save path instead.
    if (!QFileInfo(manifestPath).isFile()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Catalog collection manifest path is not a regular file" << manifestPath;
        return {};
    }
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Failed to open catalog collection manifest" << manifestPath << manifestFile.errorString();
        return {.state = State::Unreadable};
    }

    ReadResult result;
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
            << "Unreadable catalog collection manifest; the committed collection cannot be loaded" << manifestPath;
        return {.state = State::Unreadable};
    }
    result.state = State::Readable;
    return result;
}

bool SkyCatalogCollectionManifestStore::publish(const QString& directory, const Manifest& manifest)
{
    QByteArray content;
    content += QByteArrayLiteral("schemaVersion=") + QByteArray::number(manifest.schemaVersion) + '\n';
    content += QByteArrayLiteral("binarySchemaVersion=") + QByteArray::number(manifest.binarySchemaVersion) + '\n';
    content += QByteArrayLiteral("generation=") + QByteArray::number(manifest.generation) + '\n';

    return SkyCacheFiles::writeFileAtomically(path(directory), content, QStringLiteral("catalog collection manifest"));
}

quint64 SkyCatalogCollectionManifestStore::nextGeneration(
    const QString& directory, QSettings& settings, const quint64 committedGeneration
)
{
    return highestGeneration(directory, settings, committedGeneration) + 1U;
}

QString SkyCatalogCollectionManifestStore::generationGroup(const quint64 generation)
{
    return SkyCatalogSourceRecordCodec::sourcesGroup() + QLatin1Char('/') + QString::number(generation);
}

void SkyCatalogCollectionManifestStore::stageGeneration(QSettings& settings, const quint64 generation)
{
    settings.remove(generationGroup(generation));
    settings.setValue(recordsStoredKey(generation), true);
}

void SkyCatalogCollectionManifestStore::discardGeneration(QSettings& settings, const quint64 generation)
{
    settings.remove(generationGroup(generation));
}

bool SkyCatalogCollectionManifestStore::hasRecordsStored(QSettings& settings, const quint64 generation)
{
    return SkySettingsValueCodecs::readBoolSetting(settings, recordsStoredKey(generation), false);
}

bool SkyCatalogCollectionManifestStore::hasGenerationRecords(QSettings& settings)
{
    settings.beginGroup(SkyCatalogSourceRecordCodec::sourcesGroup());
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

bool SkyCatalogCollectionManifestStore::hasGenerationArtifacts(const QString& directory, QSettings& settings)
{
    return hasGenerationRecords(settings) || highestGeneration(directory, settings, 0U) > 0U;
}

std::optional<quint64> SkyCatalogCollectionManifestStore::committedGeneration(QSettings& settings)
{
    bool hasGeneration = false;
    const quint64 generation = settings.value(committedGenerationKey()).toULongLong(&hasGeneration);
    if (!hasGeneration || generation == 0U) {
        return std::nullopt;
    }
    return generation;
}

void SkyCatalogCollectionManifestStore::recordCommittedGeneration(QSettings& settings, const quint64 generation)
{
    const QString key = committedGenerationKey();
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

void SkyCatalogCollectionManifestStore::forgetCommittedGeneration(QSettings& settings)
{
    settings.remove(committedGenerationKey());
}

QStringList SkyCatalogCollectionManifestStore::cacheFilePaths(const QString& directory)
{
    QStringList paths;
    if (directory.isEmpty()) {
        return paths;
    }

    const QDir dir(directory);
    const QStringList entries =
        dir.entryList(QStringList{SkyCatalogSourceCachePaths::sidecarEntryPattern()}, QDir::Files | QDir::Readable);
    for (const QString& entryName : entries) {
        SkyCacheFiles::appendUniquePath(paths, dir.filePath(entryName));
    }
    SkyCacheFiles::appendUniquePath(paths, path(directory));
    return paths;
}

void SkyCatalogCollectionManifestStore::removeObsolete(
    QSettings& settings, const QString& directory, const quint64 committedGeneration, const QStringList& referencedPaths
)
{
    removeObsoleteFiles(directory, referencedPaths);
    removeObsoleteRecords(settings, committedGeneration);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qCWarning(skygateCatalogCacheLog)
            << "Failed to remove obsolete catalog collection records; the committed collection stays active";
    }
}

}  // namespace skygate::ui::internal
