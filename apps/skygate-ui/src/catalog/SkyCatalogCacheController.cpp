#include "SkyCatalogCacheController.hpp"

#include "CatalogParseOptions.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyContextControllerSupport.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/CatalogParseRequest.hpp"
#include "catalog/CatalogPayloadParser.hpp"

#include <QLoggingCategory>

#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCacheLog, "skygate.catalog.cache")

QString stripSavedSuffixes(const QString& sourceLabel)
{
    QString normalizedSourceLabel = sourceLabel.trimmed();
    const QString spacedSavedSuffix = QStringLiteral(" (saved)");
    const QString compactSavedSuffix = QStringLiteral("(saved)");
    while (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)
           || normalizedSourceLabel.endsWith(compactSavedSuffix, Qt::CaseInsensitive)) {
        if (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)) {
            normalizedSourceLabel.chop(spacedSavedSuffix.size());
        } else {
            normalizedSourceLabel.chop(compactSavedSuffix.size());
        }
        normalizedSourceLabel = normalizedSourceLabel.trimmed();
    }
    return normalizedSourceLabel;
}

std::string_view payloadView(const QByteArray& payload)
{
    return std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()));
}

QString savedLabel(const QString& sourceLabel, const QString& fallbackLabel)
{
    QString normalizedSourceLabel = stripSavedSuffixes(sourceLabel);
    if (normalizedSourceLabel.isEmpty()) {
        normalizedSourceLabel = fallbackLabel;
    }
    return QString("%1 (saved)").arg(normalizedSourceLabel);
}

// Two persisted records can share one instance ID (legacy two-slot migration
// of two slots configured with the same URL, or an edited settings file).
// Because the ID keys the settings group, payload sidecars, operations, and
// provenance, later duplicates receive a deterministic suffix instead of
// overwriting the first record or failing the whole restore.
QString uniqueRestoredInstanceId(
    const QString& storedInstanceId, const std::vector<SkyCatalogSourceRestoreEntry>& restoredSources
)
{
    const auto isTaken = [&restoredSources](const QString& instanceId) {
        for (const SkyCatalogSourceRestoreEntry& entry : restoredSources) {
            if (entry.record.instanceId == instanceId) {
                return true;
            }
        }
        return false;
    };

    if (!isTaken(storedInstanceId)) {
        return storedInstanceId;
    }

    int duplicateIndex = 2;
    QString candidate;
    do {
        candidate = QString("%1#%2").arg(storedInstanceId).arg(duplicateIndex++);
    } while (isTaken(candidate));
    return candidate;
}

}  // namespace

SkyCatalogCacheController::SkyCatalogCacheController(SkySettingsStore* settingsStore) : m_settingsStore(settingsStore)
{
}

bool SkyCatalogCacheController::clearCatalogCache() const
{
    return m_settingsStore != nullptr && m_settingsStore->clearCatalogCache();
}

bool SkyCatalogCacheController::clearDeepSkyCatalogCache() const
{
    return m_settingsStore != nullptr && m_settingsStore->clearDeepSkyCatalogCache();
}

bool SkyCatalogCacheController::clearSourceCache(const QString& instanceId) const
{
    return m_settingsStore != nullptr && m_settingsStore->clearCatalogSourceCache(instanceId);
}

bool SkyCatalogCacheController::clearCollectionCache() const
{
    if (m_settingsStore == nullptr) {
        return false;
    }

    const bool collectionCleared = m_settingsStore->clearCatalogCollectionCache();
    const bool legacyCleared = m_settingsStore->clearCatalogCache() && m_settingsStore->clearDeepSkyCatalogCache();
    return collectionCleared && legacyCleared;
}

SkyCatalogCacheController::DecodedCatalog SkyCatalogCacheController::decodeSourceCatalog(
    const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord, const int binarySchemaVersion
) const
{
    DecodedCatalog decoded;
    if (sourceRecord.bundled) {
        // Bundled content is reconstructed from the bundled factory on every
        // start instead of from a payload sidecar, so the record stays durable
        // configuration while the catalog data stays disposable.
        decoded.catalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
        if (decoded.catalog == nullptr) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Bundled catalog source could not be reconstructed:" << sourceRecord.instanceId;
        }
        return decoded;
    }

    if (!sourceRecord.binaryPayload.isEmpty()
        && binarySchemaVersion == static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion)) {
        decoded.catalog = skygate::ephemeris::CatalogBinaryCodec::deserialize(sourceRecord.binaryPayload);
        if (decoded.catalog == nullptr) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Saved binary catalog source cache unreadable; falling back to payload parsing";
        }
    }

    if (decoded.catalog == nullptr && !sourceRecord.payload.isEmpty()) {
        // The raw payload is reparsed with the record's own parse contract, so
        // a restored source keeps the archive member and schema hint that
        // produced the cached catalog. Older records keep their stored member
        // selection and the defined defaults for the rest; options are never
        // re-derived from a current descriptor that may have changed.
        const CatalogParseOptions parseOptions{
            .archiveMember = sourceRecord.archiveSelector, .schemaHint = sourceRecord.schemaHint
        };
        const skygate::ephemeris::CatalogParseRequest request =
            parseOptions.makeRequest(payloadView(sourceRecord.payload));

        const skygate::ephemeris::CatalogPayloadParser parser;
        auto restoredResult = parser.parseResult(request);
        if (restoredResult.isSuccess() && restoredResult.catalog != nullptr) {
            decoded.catalog = std::move(restoredResult.catalog);
            decoded.requiresBinaryUpgrade = true;
        } else {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Saved catalog source cache unreadable; restoring configuration without payload:"
                << QString::fromStdString(restoredResult.errorDetail);
        }
    }

    if (decoded.catalog == nullptr && sourceRecord.payload.isEmpty()) {
        qCWarning(skygateCatalogCacheLog).noquote()
            << "Saved catalog source cache has no payload; restoring configuration only:" << sourceRecord.instanceId;
    }
    return decoded;
}

void SkyCatalogCacheController::appendRestoredSource(
    const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord,
    DecodedCatalog decoded,
    SkyCatalogCollectionRestoreResult& result
) const
{
    SkyCatalogSourceRestoreEntry entry;
    const QString restoredInstanceId = uniqueRestoredInstanceId(sourceRecord.instanceId, result.sources);
    if (restoredInstanceId != sourceRecord.instanceId) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Duplicate persisted catalog source instance id"
                                                    << sourceRecord.instanceId << "restored as" << restoredInstanceId;
    }
    entry.record.instanceId = restoredInstanceId;
    // A bundled catalog is rebuilt live and a record without a readable
    // payload has no saved catalog behind its title; neither is a saved
    // source. The stored title is kept verbatim in both cases.
    if (decoded.catalog == nullptr || sourceRecord.bundled) {
        entry.record.title = stripSavedSuffixes(sourceRecord.title);
        if (entry.record.title.isEmpty()) {
            entry.record.title = sourceRecord.bundled ? QStringLiteral("Bundled") : restoredInstanceId;
        }
    } else {
        entry.record.title = savedLabel(sourceRecord.title, QStringLiteral("Saved"));
    }
    entry.record.version = sourceRecord.version;
    entry.record.url = sourceRecord.url;
    entry.record.policy = sourceRecord.policy;
    entry.record.enabled = sourceRecord.enabled;
    entry.record.bundled = sourceRecord.bundled;
    entry.record.catalog = std::move(decoded.catalog);
    // Only a catalog restored from a stored payload reports its own deep-sky
    // objects. A bundled record mirrors the live add path, which leaves the
    // count empty because the bundled deep-sky fallback participation supplies
    // its own count during composition; counting the rebuilt catalog here
    // would report the bundled objects twice after a restart.
    if (entry.record.catalog != nullptr && !sourceRecord.bundled) {
        entry.record.foundObjectCount =
            skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(entry.record.catalog->bodies());
    }

    entry.instance.instanceId = restoredInstanceId;
    entry.instance.descriptorId = sourceRecord.descriptorId;
    entry.instance.title = stripSavedSuffixes(sourceRecord.title);
    entry.instance.version = sourceRecord.version;
    entry.instance.urls = sourceRecord.urls;
    entry.instance.relatedDatasetUrls = sourceRecord.relatedDatasetUrls;
    entry.instance.archiveSelector = sourceRecord.archiveSelector;
    entry.instance.schemaHint = sourceRecord.schemaHint;
    entry.instance.attribution = sourceRecord.attribution;

    entry.payload = sourceRecord.payload;
    entry.requiresBinaryUpgrade = decoded.requiresBinaryUpgrade;

    // Related constellation data belongs to the star source that declared it;
    // restore it once alongside the matching source.
    if (!sourceRecord.constellationLineRows.isEmpty()
        && sourceRecord.constellationLineSchemaVersion
               >= SkyContextControllerConstants::kConstellationLineCacheSchemaVersion) {
        auto parsedLineRefs =
            SkyContextCatalogCodec::parseConstellationLineRows(payloadView(sourceRecord.constellationLineRows));
        if (!parsedLineRefs.empty()) {
            result.constellationLineRefs = std::move(parsedLineRefs);
            if (!sourceRecord.constellationAnchorGroupRows.isEmpty()) {
                result.constellationAnchorGroups = SkyContextCatalogCodec::parseConstellationAnchorGroupRows(
                    payloadView(sourceRecord.constellationAnchorGroupRows)
                );
            }
            result.constellationCount = sourceRecord.constellationCount;
        }
    } else if (sourceRecord.constellationLineSchemaVersion > 0) {
        result.resetConstellationLineRefs = true;
    }

    result.sources.push_back(std::move(entry));
    result.restored = true;
}

SkyCatalogCollectionRestoreResult
SkyCatalogCacheController::restoreFromRecords(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot) const
{
    SkyCatalogCollectionRestoreResult result;
    const int binarySchemaVersion = snapshot.binarySchemaVersion;

    // Records written before the parse contract was persisted load with the
    // defined defaults and are rewritten once in the current format.
    result.requiresRecordUpgrade =
        snapshot.schemaVersion < SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

    for (const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord : snapshot.sources) {
        DecodedCatalog decoded = decodeSourceCatalog(sourceRecord, binarySchemaVersion);
        appendRestoredSource(sourceRecord, std::move(decoded), result);
    }
    return result;
}

SkyCatalogCollectionRestoreResult SkyCatalogCacheController::migrateLegacy(
    const SkySettingsStore::CatalogCacheSnapshot& legacy,
    const int catalogPresetIndex,
    const int deepSkyCatalogPresetIndex,
    const QString& catalogUrlText,
    const QString& deepSkyCatalogUrlText
) const
{
    SkyCatalogCollectionRestoreResult result;
    result.migratedLegacy = true;

    const auto appendRecord = [&](SkySettingsStore::CatalogSourceCacheRecord record) {
        DecodedCatalog decoded = decodeSourceCatalog(record, legacy.catalogBinarySchemaVersion);
        appendRestoredSource(record, std::move(decoded), result);
    };

    const int starIndex = SkyCatalogPresets::normalizeCatalogPresetIndex(catalogPresetIndex);
    if (starIndex == 0) {
        // The legacy star slot selected the bundled source. Materialize it as
        // a bundled record so its identity, position, and Merge participation
        // survive the migration instead of silently disappearing once a
        // downloaded sibling becomes the only restored record.
        const std::optional<SkyCatalogSourceDescriptor> descriptor =
            SkyCatalogPresets::starSourceDescriptor(QStringLiteral("bundled"));
        if (descriptor.has_value()) {
            SkySettingsStore::CatalogSourceCacheRecord record;
            record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
            record.enabled = true;
            record.bundled = true;
            const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
            record.instanceId = SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
            record.descriptorId = descriptor->sourceId;
            record.title = descriptor->title;
            record.version = descriptor->version;
            record.schemaHint = descriptor->schemaHint;
            record.attribution = descriptor->attribution;
            appendRecord(std::move(record));
        }
    } else {
        SkySettingsStore::CatalogSourceCacheRecord record;
        record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        record.enabled = true;
        if (starIndex == 1) {
            const std::optional<SkyCatalogSourceDescriptor> descriptor =
                SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42"));
            if (descriptor.has_value()) {
                const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
                record.instanceId = SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
                record.descriptorId = descriptor->sourceId;
                record.title = descriptor->title;
                record.version = descriptor->version;
                record.urls = descriptor->urls;
                record.relatedDatasetUrls = descriptor->relatedDatasetUrls;
                record.archiveSelector = descriptor->archiveSelector;
                record.schemaHint = descriptor->schemaHint;
                record.attribution = descriptor->attribution;
            }
        } else {
            const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::createCustom(catalogUrlText);
            record.instanceId = SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
            record.title = stripSavedSuffixes(legacy.sourceLabel);
            if (record.title.isEmpty()) {
                record.title = instance.title;
            }
            record.urls = instance.urls;
        }
        record.payload = legacy.catalogPayload;
        record.binaryPayload = legacy.catalogBinaryPayload;
        record.constellationLineRows = legacy.constellationLineRows;
        record.constellationAnchorGroupRows = legacy.constellationAnchorGroupRows;
        record.constellationLineSchemaVersion = legacy.constellationLineSchemaVersion;
        record.constellationCount = legacy.constellationCount;
        appendRecord(std::move(record));
    }

    const int deepSkyIndex = SkyCatalogPresets::normalizeDeepSkyCatalogPresetIndex(deepSkyCatalogPresetIndex);
    if (deepSkyIndex != 0) {
        SkySettingsStore::CatalogSourceCacheRecord record;
        record.policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly;
        record.enabled = true;
        if (deepSkyIndex == 1) {
            const std::optional<SkyCatalogSourceDescriptor> descriptor =
                SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
            if (descriptor.has_value()) {
                const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
                record.instanceId = SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
                record.descriptorId = descriptor->sourceId;
                record.title = descriptor->title;
                record.version = descriptor->version;
                record.urls = descriptor->urls;
                record.relatedDatasetUrls = descriptor->relatedDatasetUrls;
                record.archiveSelector = descriptor->archiveSelector;
                record.schemaHint = descriptor->schemaHint;
                record.attribution = descriptor->attribution;
            }
        } else {
            const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::createCustom(deepSkyCatalogUrlText);
            record.instanceId = SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
            record.title = stripSavedSuffixes(legacy.deepSkySourceLabel);
            if (record.title.isEmpty()) {
                record.title = instance.title;
            }
            record.urls = instance.urls;
        }
        record.payload = legacy.deepSkyCatalogPayload;
        record.binaryPayload = legacy.deepSkyBinaryPayload;
        appendRecord(std::move(record));
    }

    return result;
}

SkyCatalogCollectionRestoreResult SkyCatalogCacheController::restoreCollection(
    const int catalogPresetIndex,
    const int deepSkyCatalogPresetIndex,
    const QString& catalogUrlText,
    const QString& deepSkyCatalogUrlText
) const
{
    if (m_settingsStore == nullptr) {
        return {};
    }

    const std::optional<SkySettingsStore::CatalogCollectionCacheSnapshot> collection =
        m_settingsStore->loadCatalogCollectionCache();
    if (collection.has_value()) {
        return restoreFromRecords(*collection);
    }

    const std::optional<SkySettingsStore::CatalogCacheSnapshot> legacy = m_settingsStore->loadCatalogCache();
    if (!legacy.has_value()) {
        return {};
    }
    return migrateLegacy(*legacy, catalogPresetIndex, deepSkyCatalogPresetIndex, catalogUrlText, deepSkyCatalogUrlText);
}

void SkyCatalogCacheController::persistCollection(const SkyCatalogCollectionPersistRequest& request) const
{
    if (m_settingsStore == nullptr) {
        return;
    }
    if (request.sources.empty()) {
        static_cast<void>(m_settingsStore->clearCatalogCollectionCache());
        return;
    }

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    for (const SkyCatalogSourcePersistEntry& source : request.sources) {
        SkySettingsStore::CatalogSourceCacheRecord record;
        record.instanceId = source.instanceId;
        record.descriptorId = source.descriptorId;
        record.title = source.title;
        record.version = source.version;
        record.url = source.url;
        record.urls = source.urls;
        record.relatedDatasetUrls = source.relatedDatasetUrls;
        record.archiveSelector = source.archiveSelector;
        record.schemaHint = source.schemaHint;
        record.attribution = source.attribution;
        record.policy = source.policy;
        record.enabled = source.enabled;
        record.bundled = source.bundled;
        if (!source.bundled) {
            record.payload = source.payload;
            if (source.catalog != nullptr) {
                record.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(source.catalog->catalog());
            }
        }
        record.constellationLineRows = source.constellationLineRows;
        record.constellationAnchorGroupRows = source.constellationAnchorGroupRows;
        record.constellationLineSchemaVersion = source.constellationLineSchemaVersion;
        record.constellationCount = source.constellationCount;
        snapshot.sources.push_back(std::move(record));
    }

    if (!m_settingsStore->saveCatalogCollectionCache(snapshot)) {
        qCWarning(skygateCatalogCacheLog).noquote() << "Failed to persist catalog source collection";
    }
}

}  // namespace skygate::ui::internal
