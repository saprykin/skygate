#include "SkyCatalogSourceCacheRecord.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourcePersistEntry.hpp"
#include "catalog/CatalogBinaryCodec.hpp"

void SkyCatalogSourceCacheRecord::applyConfiguration(const skygate::ui::internal::SkyCatalogSourceInstance& instance)
{
    descriptorId = instance.descriptorId;
    title = instance.title;
    version = instance.version;
    urls = instance.urls;
    relatedDatasetUrls = instance.relatedDatasetUrls;
    archiveSelector = instance.archiveSelector;
    schemaHint = instance.schemaHint;
    attribution = instance.attribution;
}

skygate::ui::internal::SkyCatalogSourceInstance SkyCatalogSourceCacheRecord::configuredInstance() const
{
    skygate::ui::internal::SkyCatalogSourceInstance instance;
    instance.instanceId = instanceId;
    instance.descriptorId = descriptorId;
    instance.title = title;
    instance.version = version;
    instance.urls = urls;
    instance.relatedDatasetUrls = relatedDatasetUrls;
    instance.archiveSelector = archiveSelector;
    instance.schemaHint = schemaHint;
    instance.attribution = attribution;
    return instance;
}

SkyCatalogSourceCacheRecord SkyCatalogSourceCacheRecord::fromInstance(
    const skygate::ui::internal::SkyCatalogSourceInstance& instance,
    const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    SkyCatalogSourceCacheRecord record;
    record.instanceId = instance.instanceId;
    record.applyConfiguration(instance);
    record.policy = policy;
    return record;
}

SkyCatalogSourceCacheRecord SkyCatalogSourceCacheRecord::fromDescriptor(
    const skygate::ui::internal::SkyCatalogSourceDescriptor& descriptor,
    const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    const skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(descriptor);

    SkyCatalogSourceCacheRecord record = fromInstance(instance, policy);
    record.instanceId = skygate::ui::internal::SkyCatalogSourceInstance::migratedLegacyInstanceId(instance);
    record.bundled = descriptor.bundled;
    return record;
}

SkyCatalogSourceCacheRecord
SkyCatalogSourceCacheRecord::fromPersistEntry(const skygate::ui::internal::SkyCatalogSourcePersistEntry& entry)
{
    SkyCatalogSourceCacheRecord record = entry.record;
    // A bundled source is reconstructed from the bundled factory on restore,
    // so a payload or catalog reaching this boundary must not become durable
    // data.
    if (record.bundled) {
        record.payload.clear();
        record.binaryPayload.clear();
    } else if (entry.catalog != nullptr) {
        record.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(entry.catalog->catalog());
    }
    return record;
}
