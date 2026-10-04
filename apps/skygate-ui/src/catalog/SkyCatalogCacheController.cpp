#include "SkyCatalogCacheController.hpp"

#include "SkyCatalogPresets.hpp"
#include "SkyContextControllerSupport.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogIdentity.hpp"
#include "catalog/CatalogPayloadParser.hpp"

#include <QLoggingCategory>

#include <string_view>
#include <utility>

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
    const QByteArray& payload, const QByteArray& binaryPayload, const int binarySchemaVersion
) const
{
    DecodedCatalog decoded;
    if (!binaryPayload.isEmpty()
        && binarySchemaVersion == static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion)) {
        decoded.catalog = skygate::ephemeris::CatalogBinaryCodec::deserialize(binaryPayload);
        if (decoded.catalog == nullptr) {
            qCWarning(skygateCatalogCacheLog).noquote()
                << "Saved binary catalog source cache unreadable; falling back to payload parsing";
        }
    }

    if (decoded.catalog == nullptr && !payload.isEmpty()) {
        const skygate::ephemeris::CatalogPayloadParser parser;
        auto restoredResult = parser.parseResult(payloadView(payload));
        if (restoredResult.isSuccess() && restoredResult.catalog != nullptr) {
            decoded.catalog = std::move(restoredResult.catalog);
            decoded.requiresBinaryUpgrade = true;
        } else {
            qCWarning(skygateCatalogCacheLog).noquote() << "Saved catalog source cache unreadable; ignoring source:"
                                                        << QString::fromStdString(restoredResult.errorDetail);
        }
    }
    return decoded;
}

void SkyCatalogCacheController::appendRestoredSource(
    const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord,
    DecodedCatalog decoded,
    SkyCatalogCollectionRestoreResult& result
) const
{
    if (decoded.catalog == nullptr) {
        // A damaged source is skipped without discarding its siblings.
        return;
    }

    SkyCatalogSourceRestoreEntry entry;
    entry.record.instanceId = sourceRecord.instanceId;
    entry.record.title = savedLabel(sourceRecord.title, QStringLiteral("Saved"));
    entry.record.version = sourceRecord.version;
    entry.record.url = sourceRecord.url;
    entry.record.policy = sourceRecord.policy;
    entry.record.enabled = sourceRecord.enabled;
    entry.record.catalog = std::move(decoded.catalog);
    entry.record.foundObjectCount =
        skygate::ephemeris::CatalogIdentity::countDeepSkyObjects(entry.record.catalog->bodies());

    entry.instance.instanceId = sourceRecord.instanceId;
    entry.instance.descriptorId = sourceRecord.descriptorId;
    entry.instance.title = stripSavedSuffixes(sourceRecord.title);
    entry.instance.version = sourceRecord.version;
    entry.instance.urls = sourceRecord.urls;
    entry.instance.relatedDatasetUrls = sourceRecord.relatedDatasetUrls;
    entry.instance.archiveSelector = sourceRecord.archiveSelector;

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

    for (const SkySettingsStore::CatalogSourceCacheRecord& sourceRecord : snapshot.sources) {
        DecodedCatalog decoded =
            decodeSourceCatalog(sourceRecord.payload, sourceRecord.binaryPayload, binarySchemaVersion);
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
        DecodedCatalog decoded =
            decodeSourceCatalog(record.payload, record.binaryPayload, legacy.catalogBinarySchemaVersion);
        appendRestoredSource(record, std::move(decoded), result);
    };

    const int starIndex = SkyCatalogPresets::normalizeCatalogPresetIndex(catalogPresetIndex);
    if (starIndex != 0) {
        SkySettingsStore::CatalogSourceCacheRecord record;
        record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        record.enabled = true;
        if (starIndex == 1) {
            const std::optional<SkyCatalogSourceDescriptor> descriptor =
                SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42"));
            if (descriptor.has_value()) {
                const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::fromDescriptor(*descriptor);
                record.instanceId = instance.instanceId;
                record.descriptorId = descriptor->sourceId;
                record.title = descriptor->title;
                record.version = descriptor->version;
                record.urls = descriptor->urls;
                record.relatedDatasetUrls = descriptor->relatedDatasetUrls;
                record.archiveSelector = descriptor->archiveSelector;
            }
        } else {
            const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::createCustom(catalogUrlText);
            record.instanceId = instance.instanceId;
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
                record.instanceId = instance.instanceId;
                record.descriptorId = descriptor->sourceId;
                record.title = descriptor->title;
                record.version = descriptor->version;
                record.urls = descriptor->urls;
                record.relatedDatasetUrls = descriptor->relatedDatasetUrls;
                record.archiveSelector = descriptor->archiveSelector;
            }
        } else {
            const SkyCatalogSourceInstance instance = SkyCatalogSourceInstance::createCustom(deepSkyCatalogUrlText);
            record.instanceId = instance.instanceId;
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
        record.policy = source.policy;
        record.enabled = source.enabled;
        record.payload = source.payload;
        if (source.catalog != nullptr) {
            record.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(source.catalog->catalog());
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
