#include "CatalogCollectionCacheFixture.hpp"

#include "CatalogTestPayloads.hpp"
#include "SettingsTestFixture.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QDir>
#include <QSettings>

#include <utility>

namespace skygate::ui::tests {

SkyCatalogCollectionCacheSnapshot CatalogCollectionCacheFixture::sampleCollectionSnapshot()
{
    SkyCatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkyCatalogSourceCacheRecord star;
    star.instanceId = QStringLiteral("preset:hyg_v42");
    star.descriptorId = QStringLiteral("hyg_v42");
    star.title = QStringLiteral("HYG v4.2");
    star.version = QStringLiteral("v4.2");
    star.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/lines.json")};
    star.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.order = 0;
    star.payload = skygate::ui::tests::sampleHygCsvPayload();
    star.binaryPayload = QByteArray("binary-star");
    star.constellationLineRows = "hip_1|hip_2\n";
    star.constellationAnchorGroupRows = "Demo|hip_1,hip_2\n";
    star.constellationLineSchemaVersion = 4;
    star.constellationCount = 1;
    snapshot.sources.push_back(std::move(star));

    SkyCatalogSourceCacheRecord deepSky;
    deepSky.instanceId = QStringLiteral("preset:open_ngc");
    deepSky.descriptorId = QStringLiteral("open_ngc");
    deepSky.title = QStringLiteral("OpenNGC");
    deepSky.version = QStringLiteral("v20260307");
    deepSky.urls = QStringList{QStringLiteral("https://example.test/NGC.csv")};
    deepSky.policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly;
    deepSky.enabled = true;
    deepSky.order = 1;
    deepSky.payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    deepSky.binaryPayload = QByteArray("binary-dso");
    snapshot.sources.push_back(std::move(deepSky));
    return snapshot;
}

QString CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(const SettingsTestFixture& settings)
{
    settings.resetSettingsWithCatalogCachePaths();
    const QString directory = settings.filePath(QStringLiteral("collection-cache"));
    QSettings storeSettings;
    storeSettings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), directory);
    QDir(directory).removeRecursively();
    return directory;
}

}  // namespace skygate::ui::tests
