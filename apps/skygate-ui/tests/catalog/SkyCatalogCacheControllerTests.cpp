#include "CatalogCacheTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "LogCapture.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SettingsTestFixture.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::internal::SkyCatalogCollectionRestoreResult;
using skygate::ui::internal::SkyCatalogSourcePersistEntry;
using skygate::ui::internal::SkyCatalogSourceRestoreEntry;

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeStarCatalog(const std::string& id, const std::string& name)
{
    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = id;
    star.displayName = name;
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.visualMagnitude = 3.0;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({std::move(star)});
}

SkyCatalogCollectionPersistRequest makeCollectionPersistRequest()
{
    SkyCatalogCollectionPersistRequest request;

    SkyCatalogSourcePersistEntry star;
    star.instanceId = QStringLiteral("preset:hyg_v42");
    star.descriptorId = QStringLiteral("hyg_v42");
    star.title = QStringLiteral("HYG v4.2");
    star.version = QStringLiteral("v4.2");
    star.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/lines.json")};
    star.policy = CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.payload = skygate::ui::tests::sampleHygCsvPayload();
    star.constellationLineRows = "hip_1|hip_2\n";
    star.constellationAnchorGroupRows = "Demo|hip_1,hip_2\n";
    star.constellationLineSchemaVersion = 4;
    star.constellationCount = 1;
    request.sources.push_back(std::move(star));

    SkyCatalogSourcePersistEntry deepSky;
    deepSky.instanceId = QStringLiteral("preset:open_ngc");
    deepSky.descriptorId = QStringLiteral("open_ngc");
    deepSky.title = QStringLiteral("OpenNGC");
    deepSky.version = QStringLiteral("v20260307");
    deepSky.urls = QStringList{QStringLiteral("https://example.test/NGC.csv")};
    deepSky.policy = CatalogCompositionPolicy::DeepSkyOnly;
    deepSky.enabled = true;
    deepSky.payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
    request.sources.push_back(std::move(deepSky));

    return request;
}

SkySettingsStore::CatalogCollectionCacheSnapshot makeCollectionSnapshot()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore::CatalogSourceCacheRecord star;
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

    SkySettingsStore::CatalogSourceCacheRecord deepSky;
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

SkyCatalogCollectionPersistRequest persistRequestFromRestoreResult(const SkyCatalogCollectionRestoreResult& result)
{
    SkyCatalogCollectionPersistRequest request;
    for (const SkyCatalogSourceRestoreEntry& entry : result.sources) {
        SkyCatalogSourcePersistEntry persisted;
        persisted.instanceId = entry.record.instanceId;
        persisted.descriptorId = entry.instance.descriptorId;
        persisted.title = entry.instance.title;
        persisted.version = entry.instance.version;
        persisted.urls = entry.instance.urls;
        persisted.relatedDatasetUrls = entry.instance.relatedDatasetUrls;
        persisted.archiveSelector = entry.instance.archiveSelector;
        persisted.policy = entry.record.policy;
        persisted.enabled = entry.record.enabled;
        persisted.catalog = entry.record.catalog.get();
        persisted.payload = entry.payload;
        request.sources.push_back(std::move(persisted));
    }
    return request;
}

}  // namespace

class SkyCatalogCacheControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresCollectionSourcesAndConstellationLabels();
    void restoresBinaryCatalogPayloadsWithoutUpgrade();
    void corruptBinaryPayloadFallsBackToCsvParsing();
    void legacyBinarySchemaVersionFallsBackToCsvParsing();
    void damagedSourceIsSkippedWithoutDiscardingSiblings();
    void roundTripsThreeEnabledSourcesPlusDisabledSource();
    void clearSourceCacheVersusClearCollectionCache();
    void migratesLegacyBundledCustomAndMixedConfigurations();
    void derivesDeepSkyFoundCountFromMixedSourceAfterRestore();
    void repeatedMigrationIsIdempotent();
    void failedCollectionWritePreservesPriorData();
    void logsCollectionLifecycleSummariesAtInfoLevel();

private:
    void resetSettings();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheControllerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheControllerTests")));
}

void SkyCatalogCacheControllerTests::init()
{
    resetSettings();
}

void SkyCatalogCacheControllerTests::resetSettings()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();
}

void SkyCatalogCacheControllerTests::restoresCollectionSourcesAndConstellationLabels()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeCollectionSnapshot()));

    const SkyCatalogCacheController controller(&store);
    auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(!result.migratedLegacy);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].record.instanceId, QString("preset:hyg_v42"));
    QCOMPARE(result.sources[0].record.title, QString("HYG v4.2 (saved)"));
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
    QCOMPARE(result.sources[1].record.title, QString("OpenNGC (saved)"));
    QVERIFY(result.sources[1].record.catalog != nullptr);

    QCOMPARE(result.constellationLineRefs.size(), 1U);
    QCOMPARE(result.constellationLineRefs[0].first, std::string("hip_1"));
    QCOMPARE(result.constellationLineRefs[0].second, std::string("hip_2"));
    QCOMPARE(result.constellationAnchorGroups.size(), 1U);
    QVERIFY(result.constellationCount.has_value());
    QCOMPARE(*result.constellationCount, 1U);
}

void SkyCatalogCacheControllerTests::restoresBinaryCatalogPayloadsWithoutUpgrade()
{
    auto catalog = makeStarCatalog("hip_11", "Alpha");

    SkyCatalogCollectionPersistRequest request;
    SkyCatalogSourcePersistEntry entry;
    entry.instanceId = QStringLiteral("preset:hyg_v42");
    entry.title = QStringLiteral("HYG v4.2");
    entry.urls = QStringList{QStringLiteral("https://example.test/hyg.csv.gz")};
    entry.policy = CatalogCompositionPolicy::Merge;
    entry.enabled = true;
    entry.catalog = catalog.get();
    entry.payload = skygate::ui::tests::sampleHygCsvPayload();
    request.sources.push_back(std::move(entry));

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QCOMPARE(result.sources[0].record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(std::string{result.sources[0].record.catalog->bodies()[0]->id}, std::string("hip_11"));
    QVERIFY(!result.sources[0].requiresBinaryUpgrade);
}

void SkyCatalogCacheControllerTests::corruptBinaryPayloadFallsBackToCsvParsing()
{
    auto snapshot = makeCollectionSnapshot();
    snapshot.sources[0].binaryPayload = "corrupt binary payload";
    snapshot.sources[1].binaryPayload = "also corrupt";

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, "Saved binary catalog source cache unreadable; falling back to payload parsing");
    QTest::ignoreMessage(QtWarningMsg, "Saved binary catalog source cache unreadable; falling back to payload parsing");
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QVERIFY(result.sources[1].record.catalog != nullptr);
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QVERIFY(result.sources[1].requiresBinaryUpgrade);
}

void SkyCatalogCacheControllerTests::legacyBinarySchemaVersionFallsBackToCsvParsing()
{
    auto snapshot = makeCollectionSnapshot();
    snapshot.sources[0].binaryPayload = "legacy version 2 binary payload";
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion) - 1;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[0].record.catalog->bodies().size(), std::size_t{1});
}

void SkyCatalogCacheControllerTests::damagedSourceIsSkippedWithoutDiscardingSiblings()
{
    auto snapshot = makeCollectionSnapshot();
    snapshot.sources[1].payload = "this is not a catalog";
    snapshot.sources[1].binaryPayload.clear();

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; ignoring source: Catalog payload format is not recognized."
    );
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    // The healthy star source survives; the damaged deep-sky source is skipped.
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QCOMPARE(result.sources[0].record.instanceId, QString("preset:hyg_v42"));
    QVERIFY(result.sources[0].record.catalog != nullptr);

    // The damaged record remains persisted rather than being erased silently.
    const auto stillPersisted = store.loadCatalogCollectionCache();
    QVERIFY(stillPersisted.has_value());
    QCOMPARE(stillPersisted->sources.size(), 2);
}

void SkyCatalogCacheControllerTests::roundTripsThreeEnabledSourcesPlusDisabledSource()
{
    std::vector<std::unique_ptr<skygate::ephemeris::IStarCatalog>> catalogs;
    SkyCatalogCollectionPersistRequest request;
    const int hips[] = {1, 2, 3, 4};
    for (int index = 0; index < 4; ++index) {
        const std::string id = "hip_" + std::to_string(hips[index]);
        catalogs.push_back(makeStarCatalog(id, "Star " + std::to_string(index)));

        SkyCatalogSourcePersistEntry entry;
        entry.instanceId = QStringLiteral("custom:%1").arg(index);
        entry.title = QStringLiteral("Custom %1").arg(index);
        entry.urls = QStringList{QStringLiteral("https://example.test/custom-%1.csv").arg(index)};
        entry.policy = CatalogCompositionPolicy::Merge;
        entry.enabled = index != 3;
        entry.catalog = catalogs.back().get();
        entry.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = hips[index], .properName = QByteArray("Star")});
        request.sources.push_back(std::move(entry));
    }

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{4});

    for (int index = 0; index < 4; ++index) {
        QCOMPARE(result.sources[index].record.instanceId, QStringLiteral("custom:%1").arg(index));
        QCOMPARE(result.sources[index].record.enabled, index != 3);
        QCOMPARE(result.sources[index].record.catalog->bodies().size(), std::size_t{1});
        QCOMPARE(
            std::string{result.sources[index].record.catalog->bodies()[0]->id},
            std::string("hip_") + std::to_string(hips[index])
        );
    }
}

void SkyCatalogCacheControllerTests::clearSourceCacheVersusClearCollectionCache()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeCollectionSnapshot()));

    const SkyCatalogCacheController controller(&store);
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:hyg_v42")));

    auto remaining = store.loadCatalogCollectionCache();
    QVERIFY(remaining.has_value());
    QCOMPARE(remaining->sources.size(), 1);
    QCOMPARE(remaining->sources[0].instanceId, QString("preset:open_ngc"));

    QVERIFY(controller.clearCollectionCache());
    QVERIFY(!store.loadCatalogCollectionCache().has_value());
}

void SkyCatalogCacheControllerTests::migratesLegacyBundledCustomAndMixedConfigurations()
{
    // Bundled star + bundled deep-sky has no cached content to migrate.
    {
        SkySettingsStore store;
        const SkyCatalogCacheController controller(&store);
        const auto result = controller.restoreCollection(0, 0, QString(), QString());
        QVERIFY(result.sources.empty());
        QVERIFY(!result.migratedLegacy);
    }

    // Custom star + OpenNGC deep-sky restore with their selections and content.
    {
        resetSettings();
        SkySettingsStore store;
        const SkySettingsStore::CatalogCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
        );
        QVERIFY(store.saveCatalogCache(legacy));

        const SkyCatalogCacheController controller(&store);
        const QString customUrl = QStringLiteral("https://example.test/custom-stars.csv");
        const auto result = controller.restoreCollection(2, 1, customUrl, QString());

        QVERIFY(result.migratedLegacy);
        QCOMPARE(result.sources.size(), std::size_t{2});
        QCOMPARE(
            result.sources[0].record.instanceId,
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(customUrl).instanceId
        );
        QCOMPARE(result.sources[0].record.title, QString("Custom (saved)"));
        QVERIFY(result.sources[0].record.catalog != nullptr);
        QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
        QCOMPARE(result.sources[1].record.title, QString("OpenNGC (saved)"));
        QCOMPARE(result.constellationLineRefs.size(), 1U);
    }

    // Bundled star + OpenNGC deep-sky keeps the deep-sky selection only.
    {
        resetSettings();
        SkySettingsStore store;
        SkySettingsStore::CatalogCacheSnapshot legacy;
        legacy.deepSkySourceLabel = QStringLiteral("OpenNGC");
        legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();
        QVERIFY(store.saveCatalogCache(legacy));

        const SkyCatalogCacheController controller(&store);
        const auto result = controller.restoreCollection(0, 1, QString(), QString());

        QVERIFY(result.migratedLegacy);
        QCOMPARE(result.sources.size(), std::size_t{1});
        QCOMPARE(result.sources[0].record.instanceId, QString("preset:open_ngc"));
    }
}

void SkyCatalogCacheControllerTests::derivesDeepSkyFoundCountFromMixedSourceAfterRestore()
{
    skygate::ephemeris::OwnGalaxyCelestialBody star;
    star.id = "hip_1";
    star.displayName = "Star";
    star.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    star.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};

    skygate::ephemeris::DistantCelestialBody dso;
    dso.id = "messier_031";
    dso.displayName = "M31";
    dso.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    dso.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 3.0, .declinationDeg = 4.0};
    dso.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"Andromeda Galaxy"},
    };

    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.push_back({.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = 0U});
    order.push_back({.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = 0U});
    auto mixedCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {std::move(star)}, {std::move(dso)}, std::move(order)
    );
    QVERIFY(mixedCatalog != nullptr);

    SkyCatalogCollectionPersistRequest request;
    SkyCatalogSourcePersistEntry entry;
    entry.instanceId = QStringLiteral("custom:mixed");
    entry.title = QStringLiteral("Mixed source");
    entry.urls = QStringList{QStringLiteral("https://example.test/mixed.csv")};
    entry.policy = CatalogCompositionPolicy::DeepSkyOnly;
    entry.enabled = true;
    entry.catalog = mixedCatalog.get();
    request.sources.push_back(std::move(entry));

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    // The found-object count reports only the deep-sky objects contributed by
    // the mixed source, not its stars, and stays correct after restoration.
    QCOMPARE(result.sources[0].record.foundObjectCount, std::size_t{1});
}

void SkyCatalogCacheControllerTests::repeatedMigrationIsIdempotent()
{
    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot legacy =
        skygate::ui::tests::sampleCatalogCacheSnapshot({.sourceLabel = QStringLiteral("HYG v4.2")});
    QVERIFY(store.saveCatalogCache(legacy));

    const SkyCatalogCacheController controller(&store);
    const QString customUrl = QStringLiteral("https://example.test/hyg.csv.gz");
    const auto firstResult = controller.restoreCollection(1, 1, customUrl, QString());
    QVERIFY(firstResult.migratedLegacy);
    QCOMPARE(firstResult.sources.size(), std::size_t{2});

    // Write the migrated records back, mirroring the manager's upgrade path.
    controller.persistCollection(persistRequestFromRestoreResult(firstResult));

    const auto secondResult = controller.restoreCollection(1, 1, customUrl, QString());
    QVERIFY(!secondResult.migratedLegacy);
    QCOMPARE(secondResult.sources.size(), std::size_t{2});
    QCOMPARE(secondResult.sources[0].record.instanceId, firstResult.sources[0].record.instanceId);
    QCOMPARE(secondResult.sources[1].record.instanceId, firstResult.sources[1].record.instanceId);
}

void SkyCatalogCacheControllerTests::failedCollectionWritePreservesPriorData()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeCollectionSnapshot()));

    // Point the collection cache directory at a path under a regular file so
    // sidecar file creation fails.
    const QString blockerPath = m_settings.filePath(QStringLiteral("cache-blocker"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    blocker.write("x");
    blocker.close();

    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), blockerPath + QStringLiteral("/nested/cache")
    );

    QVERIFY(!store.saveCatalogCollectionCache(makeCollectionSnapshot()));

    // The previously persisted collection is still readable.
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->sources.size(), 2);
    QVERIFY(!loaded->sources[0].payload.isEmpty());
    QVERIFY(!loaded->sources[1].payload.isEmpty());
}

void SkyCatalogCacheControllerTests::logsCollectionLifecycleSummariesAtInfoLevel()
{
    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    skygate::ui::tests::LogCapture capture(QtInfoMsg);

    QVERIFY(store.saveCatalogCollectionCache(makeCollectionSnapshot()));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QVERIFY(controller.clearCollectionCache());

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache saved: sources 2")));
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache loaded: sources 2")));
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache cleared: files")));
}

QTEST_GUILESS_MAIN(SkyCatalogCacheControllerTests)

#include "SkyCatalogCacheControllerTests.moc"
