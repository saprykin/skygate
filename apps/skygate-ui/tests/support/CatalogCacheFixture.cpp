#include "CatalogCacheFixture.hpp"
#include "CatalogTestPayloads.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/SkyCatalogCollectionPersistRequest.hpp"
#include "catalog/SkyCatalogCollectionRestoreResult.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "catalog/SkyCatalogSourcePersistEntry.hpp"
#include "catalog/SkyCatalogSourceRestoreEntry.hpp"

#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <utility>

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::internal::SkyCatalogCollectionRestoreResult;
using skygate::ui::internal::SkyCatalogSourcePersistEntry;
using skygate::ui::internal::SkyCatalogSourceRestoreEntry;

namespace {

constexpr int kCurrentCollectionSchemaVersion =
    skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;

}  // namespace

namespace skygate::ui::tests {

std::unique_ptr<skygate::ephemeris::IStarCatalog>
CatalogCacheFixture::makeStarCatalog(const std::string& id, const std::string& name)
{
    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = id;
    star.displayName = name;
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.visualMagnitude = 3.0;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({std::move(star)});
}

SkyCatalogCollectionCacheSnapshot CatalogCacheFixture::makeCollectionSnapshot()
{
    SkyCatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkyCatalogSourceCacheRecord star;
    star.instanceId = QStringLiteral("preset:hyg_v42");
    star.descriptorId = QStringLiteral("hyg_v42");
    star.title = QStringLiteral("HYG v4.2");
    star.version = QStringLiteral("v4.2");
    star.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/lines.json")};
    star.policy = CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.order = 0;
    star.payload = skygate::ui::tests::sampleHygCsvPayload();
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
    deepSky.policy = CatalogCompositionPolicy::DeepSkyOnly;
    deepSky.enabled = true;
    deepSky.order = 1;
    deepSky.payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    snapshot.sources.push_back(std::move(deepSky));

    return snapshot;
}

QString CatalogCacheFixture::firstBodyId(const skygate::ephemeris::IStarCatalog& catalog)
{
    const auto bodies = catalog.bodies();
    return bodies.empty() ? QString() : QString::fromStdString(bodies.front()->id);
}

SkyCatalogCollectionPersistRequest
CatalogCacheFixture::persistRequestFromRestoreResult(const SkyCatalogCollectionRestoreResult& result)
{
    SkyCatalogCollectionPersistRequest request;
    for (const SkyCatalogSourceRestoreEntry& entry : result.sources) {
        SkyCatalogSourcePersistEntry persisted;
        persisted.record.instanceId = entry.record.instanceId;
        persisted.record.applyConfiguration(entry.instance);
        persisted.record.policy = entry.record.policy;
        persisted.record.enabled = entry.record.enabled;
        persisted.record.bundled = entry.record.bundled;
        persisted.catalog = entry.record.catalog.get();
        persisted.record.payload = entry.payload;
        request.sources.push_back(std::move(persisted));
    }
    return request;
}

}  // namespace skygate::ui::tests
