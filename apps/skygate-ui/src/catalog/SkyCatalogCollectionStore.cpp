#include "SkyCatalogCollectionStore.hpp"
#include "SkyCatalogCollectionManifestStore.hpp"
#include "SkyCatalogSourceCachePaths.hpp"
#include "SkyCatalogSourceRecordCodec.hpp"
#include "diagnostics/SkyLoggingCategories.hpp"
#include "settings/SkyCacheFiles.hpp"
#include "settings/SkySettingsKeyBuilder.hpp"
#include "settings/SkySettingsValueCodecs.hpp"

#include <QByteArray>
#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>
#include <optional>
#include <utility>

namespace skygate::ui::internal {
namespace {

bool writeCatalogSourceFile(const QString& path, const QByteArray& payload)
{
    if (path.isEmpty() || payload.isEmpty()) {
        return true;
    }
    return SkyCacheFiles::writeFileAtomically(path, payload, QStringLiteral("catalog source cache"));
}

}  // namespace

bool SkyCatalogCollectionStore::save(const SkyCatalogCollectionCacheSnapshot& snapshot) const
{
    QSettings settings;
    const QString directory = SkyCatalogSourceCachePaths::directory(settings);

    // Every save writes a new generation. The committed generation keeps its
    // records and sidecar payload files until the new generation's manifest is
    // published, so a failure while staging any file leaves the previously
    // committed snapshot intact instead of pairing old metadata with new
    // payloads.
    const SkyCatalogCollectionManifestStore::ReadResult committedManifest =
        SkyCatalogCollectionManifestStore::read(directory);
    const quint64 committedGeneration = committedManifest.state == SkyCatalogCollectionManifestStore::State::Readable
                                            ? committedManifest.manifest.generation
                                            : 0U;
    const quint64 generation =
        SkyCatalogCollectionManifestStore::nextGeneration(directory, settings, committedGeneration);

    QStringList stagedPaths;
    const auto discardStagedFiles = [&stagedPaths]() {
        // Removal failures are already reported by the helper; staging cleanup
        // stays best effort because the committed generation is unaffected.
        (void)SkyCacheFiles::removeFiles(stagedPaths, QStringLiteral("catalog cache"));
    };
    for (const SkyCatalogSourceCacheRecord& record : snapshot.sources) {
        if (!record.payload.isEmpty()) {
            const QString rawPath =
                SkyCatalogSourceCachePaths::rawSidecarPath(directory, record.instanceId, generation);
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
            const QString binaryPath =
                SkyCatalogSourceCachePaths::binarySidecarPath(directory, record.instanceId, generation);
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
    const QString generationGroup = SkyCatalogCollectionManifestStore::generationGroup(generation);
    SkyCatalogCollectionManifestStore::stageGeneration(settings, generation);
    int order = 0;
    for (const SkyCatalogSourceCacheRecord& record : snapshot.sources) {
        SkyCatalogSourceCacheRecord orderedRecord = record;
        orderedRecord.order = order++;
        SkyCatalogSourceRecordCodec::saveRecord(
            settings,
            SkyCatalogSourceRecordCodec::sourceGroup(generationGroup, orderedRecord.instanceId),
            orderedRecord,
            SkyCatalogSourceCachePaths::rawSidecarPath(directory, orderedRecord.instanceId, generation),
            SkyCatalogSourceCachePaths::binarySidecarPath(directory, orderedRecord.instanceId, generation)
        );
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        SkyCatalogCollectionManifestStore::discardGeneration(settings, generation);
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
    const SkyCatalogCollectionManifestStore::Manifest manifest{
        .schemaVersion = snapshot.schemaVersion > 0 ? snapshot.schemaVersion : kCollectionSchemaVersion,
        .binarySchemaVersion = snapshot.binarySchemaVersion,
        .generation = generation,
    };
    if (!SkyCatalogCollectionManifestStore::publish(directory, manifest)) {
        SkyCatalogCollectionManifestStore::discardGeneration(settings, generation);
        settings.sync();
        discardStagedFiles();
        qCWarning(skygateCatalogCacheLog)
            << "Failed to publish the catalog collection generation manifest; the committed collection remains active";
        return false;
    }

    // The manifest is durable, so the commit record may now name its
    // generation. A staged generation that never reached this point leaves the
    // record pointing at the previous committed generation.
    SkyCatalogCollectionManifestStore::recordCommittedGeneration(settings, generation);

    // The new generation is durable, so the previous generation and any files
    // abandoned by an interrupted save are unreferenced now.
    SkyCatalogCollectionManifestStore::removeObsolete(settings, directory, generation, stagedPaths);
    qCInfo(skygateCatalogCacheLog).noquote() << "Catalog collection cache saved: sources" << snapshot.sources.size();
    return true;
}

SkyCatalogCollectionCacheLoadResult SkyCatalogCollectionStore::load() const
{
    QSettings settings;
    const QString directory = SkyCatalogSourceCachePaths::directory(settings);
    const QString manifestPath = SkyCatalogCollectionManifestStore::path(directory);

    SkyCatalogCollectionCacheLoadResult result;
    QString recordsGroup;
    const SkyCatalogCollectionManifestStore::ReadResult manifest = SkyCatalogCollectionManifestStore::read(directory);
    if (manifest.state == SkyCatalogCollectionManifestStore::State::Readable) {
        // The manifest only identifies the committed generation. Its records
        // must also be present in the settings file: a reset settings state
        // must not resurrect a cache directory that happens to still hold a
        // manifest.
        if (!SkyCatalogCollectionManifestStore::hasRecordsStored(settings, manifest.manifest.generation)) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest names a generation without stored records" << manifestPath;
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
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
        recordsGroup = SkyCatalogCollectionManifestStore::generationGroup(manifest.manifest.generation);
        // A readable manifest is durable commit knowledge even when an earlier
        // build published it, so it is recorded here for later manifest loss
        // and keeps the recovery point at the generation it names.
        SkyCatalogCollectionManifestStore::recordCommittedGeneration(settings, manifest.manifest.generation);
    } else {
        if (manifest.state == SkyCatalogCollectionManifestStore::State::Unreadable) {
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.diagnostic =
                QStringLiteral("The committed catalog collection manifest is unreadable: %1").arg(manifestPath);
            const std::optional<quint64> committedGeneration =
                SkyCatalogCollectionManifestStore::committedGeneration(settings);
            if (committedGeneration.has_value()) {
                result.diagnostic +=
                    QStringLiteral("; generation %1 was recorded as committed").arg(committedGeneration.value());
            }
            return result;
        }

        const std::optional<quint64> committedGeneration =
            SkyCatalogCollectionManifestStore::committedGeneration(settings);
        const bool generationDataExists =
            SkyCatalogCollectionManifestStore::hasGenerationArtifacts(directory, settings);
        // Generation-format records or sidecars without a manifest are
        // reported so the outcome below stays diagnosable; only the commit
        // record proves that a manifest was actually published.
        if (generationDataExists) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Catalog collection manifest is missing while generation-format collection data exists"
                << manifestPath;
        }
        if (committedGeneration.has_value()) {
            qCWarning(skygateCatalogCacheLog).noquote() << "Catalog collection manifest is missing while generation"
                                                        << committedGeneration.value() << "was recorded as committed";
            result.state = SkyCatalogCollectionCacheLoadResult::State::Unusable;
            result.diagnostic =
                QStringLiteral(
                    "The committed catalog collection manifest is missing while generation %1 was recorded as "
                    "committed"
                )
                    .arg(committedGeneration.value());
            return result;
        }
        // Collections written before generations were published keep their
        // version marker and records directly in the settings file.
        if (!settings.contains(SkySettingsKeyBuilder::key("catalogCollectionVersion"))) {
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
        const bool stagedGenerationGroupsExist = SkyCatalogCollectionManifestStore::hasGenerationRecords(settings);
        if (stagedGenerationGroupsExist && SkyCatalogSourceRecordCodec::hasFlatRecords(settings)) {
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
            result.diagnostic = QStringLiteral(
                "The catalog collection version marker outlived its records, which generation-format data replaced"
            );
            return result;
        }
        result.snapshot.schemaVersion = SkySettingsValueCodecs::readIntSetting(
            settings, SkySettingsKeyBuilder::key("catalogCollectionVersion"), kCollectionSchemaVersion
        );
        result.snapshot.binarySchemaVersion = SkySettingsValueCodecs::readIntSetting(
            settings, SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"), 0
        );
        recordsGroup = SkyCatalogSourceRecordCodec::sourcesGroup();
    }

    QVector<SkyCatalogSourceCacheRecord> records;
    settings.beginGroup(recordsGroup);
    const QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        if (SkyCatalogSourceRecordCodec::isGenerationGroup(group)) {
            // Staged generation records live in numeric groups; only the flat
            // sidecar-named groups under the current group are source records.
            continue;
        }
        SkyCatalogSourceCacheRecord record = SkyCatalogSourceRecordCodec::loadRecord(settings, group);
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
    const QString configuredDirectory = SkyCatalogSourceCachePaths::directory(settings);
    const QString defaultDirectory = SkyCatalogSourceCachePaths::defaultDirectory();

    QStringList cachePaths = SkyCatalogCollectionManifestStore::cacheFilePaths(configuredDirectory);
    for (const QString& cachePath : SkyCatalogCollectionManifestStore::cacheFilePaths(defaultDirectory)) {
        SkyCacheFiles::appendUniquePath(cachePaths, cachePath);
    }
    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));

    settings.remove(SkyCatalogSourceRecordCodec::sourcesGroup());
    settings.remove(SkySettingsKeyBuilder::key("catalogCollectionVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogBinarySchemaVersion"));
    settings.remove(SkySettingsKeyBuilder::key("catalogCollectionCachePath"));
    SkyCatalogCollectionManifestStore::forgetCommittedGeneration(settings);
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
    const QString directory = SkyCatalogSourceCachePaths::directory(settings);
    const SkyCatalogCollectionManifestStore::ReadResult manifest = SkyCatalogCollectionManifestStore::read(directory);
    // A published generation keeps its records in its own settings namespace;
    // records written before generations were published keep theirs directly
    // under catalogSources.
    const QString parentGroup = manifest.state == SkyCatalogCollectionManifestStore::State::Readable
                                    ? SkyCatalogCollectionManifestStore::generationGroup(manifest.manifest.generation)
                                    : SkyCatalogSourceRecordCodec::sourcesGroup();
    const QString group = SkyCatalogSourceRecordCodec::sourceGroup(parentGroup, instanceId);
    settings.beginGroup(group);
    const QString rawPath = settings.value(SkyCatalogSourceRecordCodec::payloadPathKey()).toString();
    const QString binaryPath = settings.value(SkyCatalogSourceRecordCodec::binaryPayloadPathKey()).toString();
    settings.endGroup();

    QStringList cachePaths;
    SkyCacheFiles::appendUniquePath(cachePaths, rawPath);
    SkyCacheFiles::appendUniquePath(cachePaths, binaryPath);
    const bool removedAllCacheFiles = SkyCacheFiles::removeFiles(cachePaths, QStringLiteral("catalog cache"));

    // Payload eviction keeps the configured record: identity, order, enabled
    // state, descriptor, and parse contract stay durable. The disposable
    // payload references and the owned related dataset they carry are dropped
    // together, so a restart restores the source as configured with an
    // unavailable payload instead of deleting its configuration. Only a later
    // accepted load writes payload references again.
    SkyCatalogSourceRecordCodec::evictPayload(settings, group);
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
