#pragma once

#include "SkySettingsStore.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QString>

#include <string>
#include <vector>

namespace skygate::ui::tests {

// Queries over persisted catalog source facts: committed collection records,
// their payload sidecar paths, and their durable titles.
class CatalogSourceFactsQuery final {
public:
    [[nodiscard]] static bool
    containsInstanceId(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId);
    [[nodiscard]] static const SkySettingsStore::CatalogSourceCacheRecord*
    findRecord(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId);
    [[nodiscard]] static QString
    sidecarPath(const QString& directory, const QString& instanceId, const QString& extension);
    [[nodiscard]] static QString durableTitle(const QString& title);
    [[nodiscard]] static std::vector<skygate::ephemeris::ConstellationLineRef>
    relatedLineRefs(const SkySettingsStore::CatalogSourceCacheRecord& record);
    [[nodiscard]] static bool relatedLineRefsContainHip(
        const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs, const std::string& hipId
    );
};

}  // namespace skygate::ui::tests
