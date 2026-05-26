#pragma once

#include "CatalogTestPayloads.hpp"
#include "SkySettingsStore.hpp"

#include <cstddef>

namespace skygate::ui::tests {

struct CatalogCacheSnapshotOptions final {
    QString sourceLabel = QStringLiteral("Saved");
    QString deepSkySourceLabel = QStringLiteral("Saved OpenNGC");
    QByteArray constellationLineRows = "hip_1|hip_2\n";
    QByteArray constellationAnchorGroupRows = "Demo|hip_1,hip_2\n";
    int constellationLineSchemaVersion = 4;
    std::size_t constellationCount = 1;
};

[[nodiscard]] inline SkySettingsStore::CatalogCacheSnapshot
sampleCatalogCacheSnapshot(const CatalogCacheSnapshotOptions& options = {})
{
    SkySettingsStore::CatalogCacheSnapshot snapshot;
    snapshot.sourceLabel = options.sourceLabel;
    snapshot.catalogPayload = sampleHygCsvPayload();
    snapshot.deepSkySourceLabel = options.deepSkySourceLabel;
    snapshot.deepSkyCatalogPayload = sampleCompactOpenNgcCsvPayload();
    snapshot.constellationLineRows = options.constellationLineRows;
    snapshot.constellationAnchorGroupRows = options.constellationAnchorGroupRows;
    snapshot.constellationLineSchemaVersion = options.constellationLineSchemaVersion;
    snapshot.constellationCount = options.constellationCount;
    return snapshot;
}

}  // namespace skygate::ui::tests
