#include "SkyCatalogSourcePersistEntry.hpp"

namespace skygate::ui::internal {

SkyCatalogSourcePersistEntry SkyCatalogSourcePersistEntry::fromRuntimeRecord(const SkyCatalogSourceRecord& source)
{
    SkyCatalogSourcePersistEntry entry;
    entry.record.instanceId = source.instanceId;
    entry.record.title = source.title;
    entry.record.version = source.version;
    entry.record.policy = source.policy;
    entry.record.enabled = source.enabled;
    entry.record.bundled = source.bundled;
    return entry;
}

}  // namespace skygate::ui::internal
