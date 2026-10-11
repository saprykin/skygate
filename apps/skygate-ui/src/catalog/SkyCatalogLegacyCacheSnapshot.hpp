#pragma once

#include <QByteArray>
#include <QString>

#include <cstddef>

// Legacy two-slot catalog cache. Retained so existing installations can be
// migrated to SkyCatalogCollectionCacheSnapshot by
// SkyCatalogCacheController::migrateLegacy; no new writes use it.
struct SkyCatalogLegacyCacheSnapshot final {
    QString sourceLabel;
    QByteArray catalogPayload;
    QByteArray catalogBinaryPayload;
    QString deepSkySourceLabel;
    QByteArray deepSkyCatalogPayload;
    QByteArray deepSkyBinaryPayload;
    QByteArray constellationLineRows;
    QByteArray constellationAnchorGroupRows;
    int constellationLineSchemaVersion = 0;
    int catalogBinarySchemaVersion = 0;
    std::size_t constellationCount = 0;
};
