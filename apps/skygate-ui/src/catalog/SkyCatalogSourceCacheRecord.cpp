#include "SkyCatalogSourceCacheRecord.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourcePersistEntry.hpp"
#include "catalog/CatalogBinaryCodec.hpp"

SkyCatalogSourceCacheRecord SkyCatalogSourceCacheRecord::fromDescriptor(
    const skygate::ui::internal::SkyCatalogSourceDescriptor& descriptor,
    const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    const skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(descriptor);

    SkyCatalogSourceCacheRecord record;
    record.instanceId = skygate::ui::internal::SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
    record.descriptorId = descriptor.sourceId;
    record.title = descriptor.title;
    record.version = descriptor.version;
    record.urls = descriptor.urls;
    record.relatedDatasetUrls = descriptor.relatedDatasetUrls;
    record.archiveSelector = descriptor.archiveSelector;
    record.schemaHint = descriptor.schemaHint;
    record.attribution = descriptor.attribution;
    record.policy = policy;
    record.bundled = descriptor.bundled;
    return record;
}

SkyCatalogSourceCacheRecord
SkyCatalogSourceCacheRecord::fromPersistEntry(const skygate::ui::internal::SkyCatalogSourcePersistEntry& entry)
{
    SkyCatalogSourceCacheRecord record;
    record.instanceId = entry.instanceId;
    record.descriptorId = entry.descriptorId;
    record.title = entry.title;
    record.version = entry.version;
    record.url = entry.url;
    record.urls = entry.urls;
    record.relatedDatasetUrls = entry.relatedDatasetUrls;
    record.archiveSelector = entry.archiveSelector;
    record.schemaHint = entry.schemaHint;
    record.attribution = entry.attribution;
    record.policy = entry.policy;
    record.enabled = entry.enabled;
    record.bundled = entry.bundled;
    // A bundled source is reconstructed from the bundled factory on restore,
    // so a payload or catalog reaching this boundary must not become durable
    // data.
    if (!entry.bundled) {
        record.payload = entry.payload;
        if (entry.catalog != nullptr) {
            record.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(entry.catalog->catalog());
        }
    }
    record.constellationLineRows = entry.constellationLineRows;
    record.constellationAnchorGroupRows = entry.constellationAnchorGroupRows;
    record.constellationLineSchemaVersion = entry.constellationLineSchemaVersion;
    record.constellationCount = entry.constellationCount;
    return record;
}
