#include "CatalogCacheTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "LogCapture.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogSourceType;
using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::internal::SkyCatalogCollectionRestoreResult;
using skygate::ui::internal::SkyCatalogSourcePersistEntry;
using skygate::ui::internal::SkyCatalogSourceRestoreEntry;

constexpr int kCurrentCollectionSchemaVersion =
    skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
constexpr int kCurrentConstellationSchemaVersion =
    skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;

const QString kArchiveSourceUrl = QStringLiteral("https://example.test/catalogs.zip");
const QString kStarsMember = QStringLiteral("catalog/hyg.csv");
const QString kDeepSkyMember = QStringLiteral("catalog/ngc.csv");

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

// A persisted source whose raw payload is the multi-member archive from
// V2-05, with one member named by the descriptor's archive selection.
SkySettingsStore::CatalogCollectionCacheSnapshot makeArchiveCollectionSnapshot()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore::CatalogSourceCacheRecord archive;
    archive.instanceId = QStringLiteral("custom:archive");
    archive.descriptorId = QStringLiteral("archive_demo");
    archive.title = QStringLiteral("Archive Catalog");
    archive.version = QStringLiteral("v2026.1");
    archive.urls = QStringList{kArchiveSourceUrl};
    archive.archiveSelector = kDeepSkyMember;
    archive.schemaHint = CatalogSourceType::OpenNgcCsv;
    archive.attribution = QStringLiteral("Demo archive attribution");
    archive.policy = CatalogCompositionPolicy::Merge;
    archive.enabled = true;
    archive.order = 0;
    archive.payload = skygate::ui::tests::sampleTwoMemberCatalogZip();
    snapshot.sources.push_back(std::move(archive));

    return snapshot;
}

SkySettingsStore::CatalogSourceCacheRecord makeArchiveSourceRecord(const QString& instanceId)
{
    SkySettingsStore::CatalogSourceCacheRecord record;
    record.instanceId = instanceId;
    record.descriptorId = QStringLiteral("archive_demo");
    record.title = QStringLiteral("Archive Catalog");
    record.version = QStringLiteral("v2026.1");
    record.urls = QStringList{kArchiveSourceUrl};
    record.policy = CatalogCompositionPolicy::Merge;
    record.enabled = true;
    record.payload = skygate::ui::tests::sampleTwoMemberCatalogZip();
    return record;
}

// Removes the keys a record written before the parse contract was persisted
// cannot have, mirroring an older settings file.
QStringList removePersistedParseOptions()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList recordGroups = settings.childGroups();
    settings.endGroup();

    for (const QString& group : recordGroups) {
        settings.beginGroup(QStringLiteral("catalogSources/") + group);
        settings.remove(QStringLiteral("schemaHint"));
        settings.remove(QStringLiteral("attribution"));
        settings.endGroup();
    }
    settings.sync();
    return recordGroups;
}

QString firstBodyId(const skygate::ephemeris::IStarCatalog& catalog)
{
    const auto bodies = catalog.bodies();
    return bodies.empty() ? QString() : QString::fromStdString(bodies.front()->id);
}

// A persisted collection in the current record format: two sources whose raw
// payloads must be reparsed because they carry no binary sidecars.
SkySettingsStore::CatalogCollectionCacheSnapshot makeCollectionSnapshot()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion;
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
        persisted.schemaHint = entry.instance.schemaHint;
        persisted.attribution = entry.instance.attribution;
        persisted.policy = entry.record.policy;
        persisted.enabled = entry.record.enabled;
        persisted.bundled = entry.record.bundled;
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
    void roundTripsEachSourcesOwnRelatedDataset();
    void adoptsPriorSnapshotWithOneRelatedOwner();
    void dropsPriorDuplicatedRelatedPayloads();
    void corruptRelatedPayloadLeavesSiblingsIntact();
    void restoresBinaryCatalogPayloadsWithoutUpgrade();
    void corruptBinaryPayloadFallsBackToCsvParsing();
    void legacyBinarySchemaVersionFallsBackToCsvParsing();
    void damagedSourceKeepsConfigurationWithoutDiscardingSiblings();
    void restoresSelectedArchiveMemberWhenBinaryCacheIsMissing();
    void restoresSelectedArchiveMemberWhenBinaryCacheIsOutdated();
    void restoresSelectedArchiveMemberWhenBinaryCacheIsCorrupt();
    void parseOptionFailuresKeepDamagedRecordsAsConfiguration();
    void unreadableSchemaHintFallsBackToDetection();
    void roundTripsParseOptionsDescriptorMetadataAndAttribution();
    void persistsAndRestoresBundledRecordsFromFactory();
    void olderRecordFormatRestoresDefinedDefaults();
    void roundTripsThreeEnabledSourcesPlusDisabledSource();
    void clearSourceCacheVersusClearCollectionCache();
    void migratesLegacyBundledCustomAndMixedConfigurations();
    void derivesDeepSkyFoundCountFromMixedSourceAfterRestore();
    void repeatedMigrationIsIdempotent();
    void emptyCollectionAfterMigrationDoesNotRestoreLegacySources();
    void clearingPayloadCacheDoesNotReAddLegacySources();
    void failedMigrationCommitKeepsLegacyCacheReadable();
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

    // Related constellation data is owned by the record that stored it.
    const skygate::ui::internal::SkyCatalogConstellationStore& starConstellationData =
        result.sources[0].record.constellationData;
    QCOMPARE(starConstellationData.lineRefs().size(), 1U);
    QCOMPARE(starConstellationData.lineRefs()[0].first, std::string("hip_1"));
    QCOMPARE(starConstellationData.lineRefs()[0].second, std::string("hip_2"));
    QCOMPARE(starConstellationData.anchorGroups().size(), 1U);
    QCOMPARE(starConstellationData.count(), 1U);
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
}

void SkyCatalogCacheControllerTests::roundTripsEachSourcesOwnRelatedDataset()
{
    auto sourceACatalog = makeStarCatalog("hip_11", "Owner A Star");
    auto sourceBCatalog = makeStarCatalog("hip_22", "Owner B Star");

    SkyCatalogCollectionPersistRequest request;
    SkyCatalogSourcePersistEntry sourceA;
    sourceA.instanceId = QStringLiteral("custom:owner-a");
    sourceA.title = QStringLiteral("Owner A");
    sourceA.urls = QStringList{QStringLiteral("https://example.test/owner-a.csv")};
    sourceA.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/owner-a-lines.json")};
    sourceA.policy = CatalogCompositionPolicy::Merge;
    sourceA.enabled = true;
    sourceA.catalog = sourceACatalog.get();
    sourceA.payload = skygate::ui::tests::sampleHygCsvPayload({.id = 11, .hip = 11, .properName = "Owner A Star"});
    sourceA.constellationLineRows = "hip_11|hip_12\nhip_12|hip_13\n";
    sourceA.constellationAnchorGroupRows = "Orion|hip_11,hip_12\n";
    sourceA.constellationLineSchemaVersion = kCurrentConstellationSchemaVersion;
    sourceA.constellationCount = 1;
    request.sources.push_back(std::move(sourceA));

    SkyCatalogSourcePersistEntry sourceB;
    sourceB.instanceId = QStringLiteral("custom:owner-b");
    sourceB.title = QStringLiteral("Owner B");
    sourceB.urls = QStringList{QStringLiteral("https://example.test/owner-b.csv")};
    sourceB.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/owner-b-lines.json")};
    sourceB.policy = CatalogCompositionPolicy::Merge;
    sourceB.enabled = true;
    sourceB.catalog = sourceBCatalog.get();
    sourceB.payload = skygate::ui::tests::sampleHygCsvPayload({.id = 22, .hip = 22, .properName = "Owner B Star"});
    sourceB.constellationLineRows = "hip_21|hip_22\n";
    sourceB.constellationAnchorGroupRows = "Lyra|hip_21,hip_22\n";
    sourceB.constellationLineSchemaVersion = kCurrentConstellationSchemaVersion;
    sourceB.constellationCount = 2;
    request.sources.push_back(std::move(sourceB));

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{2});

    // Each record restores only the dataset its own download produced.
    const auto& sourceAConstellationData = result.sources[0].record.constellationData;
    QCOMPARE(sourceAConstellationData.lineRefs().size(), 2U);
    QCOMPARE(sourceAConstellationData.lineRefs()[0].first, std::string("hip_11"));
    QCOMPARE(sourceAConstellationData.lineRefs()[0].second, std::string("hip_12"));
    QCOMPARE(sourceAConstellationData.anchorGroups().size(), 1U);
    QCOMPARE(sourceAConstellationData.anchorGroups()[0].first, std::string("Orion"));
    QCOMPARE(sourceAConstellationData.count(), 1U);

    const auto& sourceBConstellationData = result.sources[1].record.constellationData;
    QCOMPARE(sourceBConstellationData.lineRefs().size(), 1U);
    QCOMPARE(sourceBConstellationData.lineRefs()[0].first, std::string("hip_21"));
    QCOMPARE(sourceBConstellationData.lineRefs()[0].second, std::string("hip_22"));
    QCOMPARE(sourceBConstellationData.anchorGroups().size(), 1U);
    QCOMPARE(sourceBConstellationData.anchorGroups()[0].first, std::string("Lyra"));
    QCOMPARE(sourceBConstellationData.count(), 2U);
}

void SkyCatalogCacheControllerTests::adoptsPriorSnapshotWithOneRelatedOwner()
{
    // A record written before the per-source related format holds a copy of
    // the composed collection-wide view. With one such record in the snapshot
    // no other record could have supplied the copy, so that record is its
    // owner and the payload is migrated instead of discarded.
    auto snapshot = makeCollectionSnapshot();
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion - 1;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].record.constellationData.lineRefs().size(), 1U);
    QCOMPARE(result.sources[0].record.constellationData.anchorGroups().size(), 1U);
    QCOMPARE(result.sources[0].record.constellationData.count(), 1U);
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
}

void SkyCatalogCacheControllerTests::dropsPriorDuplicatedRelatedPayloads()
{
    // The prior writer serialized the same composed collection-wide view into
    // every eligible source record. Identical copies in several records cannot
    // establish an owner, so none of them is attributed to a record.
    auto snapshot = makeCollectionSnapshot();
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion - 1;
    snapshot.sources[1].constellationLineRows = snapshot.sources[0].constellationLineRows;
    snapshot.sources[1].constellationAnchorGroupRows = snapshot.sources[0].constellationAnchorGroupRows;
    snapshot.sources[1].constellationLineSchemaVersion = snapshot.sources[0].constellationLineSchemaVersion;
    snapshot.sources[1].constellationCount = snapshot.sources[0].constellationCount;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicated pre-migration related constellation data"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicated pre-migration related constellation data"));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QVERIFY(result.sources[0].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[0].record.constellationData.anchorGroups().empty());
    QCOMPARE(result.sources[0].record.constellationData.count(), std::size_t{0});
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[1].record.constellationData.anchorGroups().empty());
    QCOMPARE(result.sources[1].record.constellationData.count(), std::size_t{0});
}

void SkyCatalogCacheControllerTests::corruptRelatedPayloadLeavesSiblingsIntact()
{
    auto snapshot = makeCollectionSnapshot();
    // The star record's line payload is unreadable...
    snapshot.sources[0].constellationLineRows = "not a related line payload";
    // ... and the deep-sky record's anchor payload is unreadable.
    snapshot.sources[1].constellationLineRows = "hip_51|hip_52\n";
    snapshot.sources[1].constellationAnchorGroupRows = "not a related anchor payload";
    snapshot.sources[1].constellationLineSchemaVersion = kCurrentConstellationSchemaVersion;
    snapshot.sources[1].constellationCount = 1;

    // A third, healthy record carries a distinguishable dataset.
    SkySettingsStore::CatalogSourceCacheRecord healthy;
    healthy.instanceId = QStringLiteral("custom:healthy-owner");
    healthy.title = QStringLiteral("Healthy Owner");
    healthy.urls = QStringList{QStringLiteral("https://example.test/healthy-owner.csv")};
    healthy.policy = CatalogCompositionPolicy::Merge;
    healthy.enabled = true;
    healthy.order = 2;
    healthy.payload = skygate::ui::tests::sampleHygCsvPayload();
    healthy.constellationLineRows = "hip_61|hip_62\n";
    healthy.constellationAnchorGroupRows = "Draco|hip_61,hip_62\n";
    healthy.constellationLineSchemaVersion = kCurrentConstellationSchemaVersion;
    healthy.constellationCount = 3;
    snapshot.sources.push_back(std::move(healthy));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Saved related constellation dataset is unreadable"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Saved related constellation dataset is unreadable"));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{3});
    // A record whose payload cannot be parsed restores without related data,
    // including no partial dataset from the readable half.
    QVERIFY(result.sources[0].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[0].record.constellationData.anchorGroups().empty());
    QVERIFY(result.sources[1].record.constellationData.lineRefs().empty());
    QVERIFY(result.sources[1].record.constellationData.anchorGroups().empty());
    // The healthy owner's dataset is unaffected.
    QCOMPARE(result.sources[2].record.constellationData.lineRefs().size(), 1U);
    QCOMPARE(result.sources[2].record.constellationData.lineRefs()[0].first, std::string("hip_61"));
    QCOMPARE(result.sources[2].record.constellationData.anchorGroups().size(), 1U);
    QCOMPARE(result.sources[2].record.constellationData.anchorGroups()[0].first, std::string("Draco"));
    QCOMPARE(result.sources[2].record.constellationData.count(), 3U);
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

void SkyCatalogCacheControllerTests::damagedSourceKeepsConfigurationWithoutDiscardingSiblings()
{
    auto snapshot = makeCollectionSnapshot();
    snapshot.sources[1].payload = "this is not a catalog";
    snapshot.sources[1].binaryPayload.clear();

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; restoring configuration without payload: Catalog payload format is not "
        "recognized."
    );
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    // The healthy star source restores with its catalog; the damaged deep-sky
    // source keeps its identity, order, policy, and participation state as an
    // explicit payload-less record instead of being dropped.
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].record.instanceId, QString("preset:hyg_v42"));
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
    QCOMPARE(result.sources[1].record.policy, CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(result.sources[1].record.enabled);
    QVERIFY(result.sources[1].record.catalog == nullptr);
    QCOMPARE(result.sources[1].instance.urls, QStringList{QStringLiteral("https://example.test/NGC.csv")});

    // The damaged record remains persisted rather than being erased silently.
    const auto stillPersisted = store.loadCatalogCollectionCache();
    QVERIFY(stillPersisted.has_value());
    QCOMPARE(stillPersisted->sources.size(), 2);
}

void SkyCatalogCacheControllerTests::restoresSelectedArchiveMemberWhenBinaryCacheIsMissing()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeArchiveCollectionSnapshot()));

    const SkyCatalogCacheController controller(&store);
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QVERIFY(!result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[0].record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheControllerTests::restoresSelectedArchiveMemberWhenBinaryCacheIsOutdated()
{
    // A binary sidecar written by an older binary schema is not trusted, so the
    // raw archive must be reparsed with the stored member selection. The
    // outdated sidecar deliberately holds the other archive member.
    auto snapshot = makeArchiveCollectionSnapshot();
    const auto otherMemberCatalog = makeStarCatalog("hip_42", "Sirius");
    snapshot.sources[0].binaryPayload =
        skygate::ephemeris::CatalogBinaryCodec::serialize(otherMemberCatalog->catalog());
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion) - 1;

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[0].record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheControllerTests::restoresSelectedArchiveMemberWhenBinaryCacheIsCorrupt()
{
    auto snapshot = makeArchiveCollectionSnapshot();
    snapshot.sources[0].binaryPayload = "corrupt archive binary payload";

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, "Saved binary catalog source cache unreadable; falling back to payload parsing");
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QVERIFY(result.sources[0].requiresBinaryUpgrade);
    QCOMPARE(result.sources[0].record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheControllerTests::parseOptionFailuresKeepDamagedRecordsAsConfiguration()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore::CatalogSourceCacheRecord missingMember =
        makeArchiveSourceRecord(QStringLiteral("custom:missing"));
    missingMember.archiveSelector = QStringLiteral("catalog/missing.csv");
    missingMember.schemaHint = CatalogSourceType::OpenNgcCsv;
    missingMember.order = 0;
    snapshot.sources.push_back(std::move(missingMember));

    SkySettingsStore::CatalogSourceCacheRecord hintMismatch =
        makeArchiveSourceRecord(QStringLiteral("custom:hint-mismatch"));
    hintMismatch.schemaHint = CatalogSourceType::OpenNgcCsv;
    hintMismatch.urls = QStringList{QStringLiteral("https://example.test/stars.csv")};
    hintMismatch.payload = skygate::ui::tests::sampleHygCsvPayload();
    hintMismatch.order = 1;
    snapshot.sources.push_back(std::move(hintMismatch));

    SkySettingsStore::CatalogSourceCacheRecord valid = makeArchiveSourceRecord(QStringLiteral("custom:valid"));
    valid.archiveSelector = kStarsMember;
    valid.schemaHint = CatalogSourceType::HygCsv;
    valid.order = 2;
    snapshot.sources.push_back(std::move(valid));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload does not contain member 'catalog/missing.csv'."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; restoring configuration without payload: ZIP catalog payload does "
        "not contain member 'catalog/missing.csv'."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload schema 'HYG CSV' does not match the expected schema hint "
        "'OpenNGC CSV'."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; restoring configuration without payload: Catalog payload schema "
        "'HYG CSV' does not match the expected schema hint 'OpenNGC CSV'."
    );
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    // The records whose stored parse contract does not match their payload
    // keep their configuration in place; only the valid record restores a
    // catalog.
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{3});
    QCOMPARE(result.sources[0].record.instanceId, QString("custom:missing"));
    QVERIFY(result.sources[0].record.catalog == nullptr);
    QCOMPARE(result.sources[0].instance.urls, QStringList{kArchiveSourceUrl});
    QCOMPARE(result.sources[1].record.instanceId, QString("custom:hint-mismatch"));
    QVERIFY(result.sources[1].record.catalog == nullptr);
    QCOMPARE(result.sources[1].instance.urls, QStringList{QStringLiteral("https://example.test/stars.csv")});
    QCOMPARE(result.sources[2].record.instanceId, QString("custom:valid"));
    QVERIFY(result.sources[2].record.catalog != nullptr);
    QCOMPARE(firstBodyId(*result.sources[2].record.catalog), QStringLiteral("hip_42"));

    // The failing records stay persisted; a bad parse does not erase them.
    const auto stillPersisted = store.loadCatalogCollectionCache();
    QVERIFY(stillPersisted.has_value());
    QCOMPARE(stillPersisted->sources.size(), 3);
}

void SkyCatalogCacheControllerTests::unreadableSchemaHintFallsBackToDetection()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeArchiveCollectionSnapshot()));

    // A garbled hint must fall back to "no hint" so the payload is still read
    // by detection instead of failing with a synthetic mismatch.
    QSettings settings;
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList recordGroups = settings.childGroups();
    settings.endGroup();
    QCOMPARE(recordGroups.size(), 1);
    settings.beginGroup(QStringLiteral("catalogSources/") + recordGroups.first());
    settings.setValue(QStringLiteral("schemaHint"), 9);
    settings.endGroup();
    settings.sync();

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid catalog source schema hint .*"));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{1});
    QCOMPARE(result.sources[0].instance.schemaHint, CatalogSourceType::Unknown);
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QCOMPARE(firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheControllerTests::roundTripsParseOptionsDescriptorMetadataAndAttribution()
{
    SkyCatalogCollectionPersistRequest request;
    SkyCatalogSourcePersistEntry entry;
    entry.instanceId = QStringLiteral("custom:archive");
    entry.descriptorId = QStringLiteral("archive_demo");
    entry.title = QStringLiteral("Archive Catalog");
    entry.version = QStringLiteral("v2026.1");
    entry.urls = QStringList{kArchiveSourceUrl};
    entry.archiveSelector = kDeepSkyMember;
    entry.schemaHint = CatalogSourceType::OpenNgcCsv;
    entry.attribution = QStringLiteral("Demo archive attribution");
    entry.policy = CatalogCompositionPolicy::Merge;
    entry.enabled = true;
    entry.payload = skygate::ui::tests::sampleTwoMemberCatalogZip();
    request.sources.push_back(std::move(entry));

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto stored = store.loadCatalogCollectionCache();
    QVERIFY(stored.has_value());
    QCOMPARE(stored->schemaVersion, kCurrentCollectionSchemaVersion);
    QCOMPARE(stored->sources.size(), 1);
    QCOMPARE(stored->sources[0].instanceId, QString("custom:archive"));
    QCOMPARE(stored->sources[0].descriptorId, QString("archive_demo"));
    QCOMPARE(stored->sources[0].version, QString("v2026.1"));
    QCOMPARE(stored->sources[0].archiveSelector, kDeepSkyMember);
    QCOMPARE(stored->sources[0].schemaHint, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(stored->sources[0].attribution, QString("Demo archive attribution"));

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QVERIFY(!result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{1});

    const SkyCatalogSourceRestoreEntry& restored = result.sources[0];
    QCOMPARE(restored.instance.instanceId, QString("custom:archive"));
    QCOMPARE(restored.instance.descriptorId, QString("archive_demo"));
    QCOMPARE(restored.instance.version, QString("v2026.1"));
    QCOMPARE(restored.instance.archiveSelector, kDeepSkyMember);
    QCOMPARE(restored.instance.schemaHint, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(restored.instance.attribution, QString("Demo archive attribution"));
    QCOMPARE(restored.record.version, QString("v2026.1"));
    QVERIFY(restored.record.catalog != nullptr);
    QCOMPARE(restored.record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(firstBodyId(*restored.record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheControllerTests::persistsAndRestoresBundledRecordsFromFactory()
{
    auto bundledCatalog = makeStarCatalog("hip_42", "Sirius");
    auto downloadedCatalog = makeStarCatalog("hip_43", "Vega");

    SkyCatalogCollectionPersistRequest request;

    SkyCatalogSourcePersistEntry bundled;
    bundled.instanceId = QStringLiteral("preset:bundled");
    bundled.descriptorId = QStringLiteral("bundled");
    bundled.title = QStringLiteral("Bundled");
    bundled.policy = CatalogCompositionPolicy::Merge;
    bundled.enabled = false;
    bundled.bundled = true;
    // A bundled record is configuration only: a supplied payload or catalog
    // must not be persisted as durable data.
    bundled.payload = QByteArray("stale bundled payload");
    bundled.catalog = bundledCatalog.get();
    request.sources.push_back(std::move(bundled));

    SkyCatalogSourcePersistEntry downloaded;
    downloaded.instanceId = QStringLiteral("custom:stars");
    downloaded.title = QStringLiteral("Custom stars");
    downloaded.urls = QStringList{QStringLiteral("https://example.test/stars.csv")};
    downloaded.policy = CatalogCompositionPolicy::DeepSkyOnly;
    downloaded.enabled = true;
    downloaded.catalog = downloadedCatalog.get();
    downloaded.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 43, .properName = "Vega"});
    request.sources.push_back(std::move(downloaded));

    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    controller.persistCollection(request);

    const auto stored = store.loadCatalogCollectionCache();
    QVERIFY(stored.has_value());
    QCOMPARE(stored->sources.size(), 2);
    QVERIFY(stored->sources[0].bundled);
    QVERIFY(stored->sources[0].payload.isEmpty());
    QVERIFY(stored->sources[0].binaryPayload.isEmpty());
    QCOMPARE(stored->sources[0].policy, CatalogCompositionPolicy::Merge);
    QVERIFY(!stored->sources[0].enabled);
    QVERIFY(!stored->sources[1].bundled);
    QVERIFY(!stored->sources[1].payload.isEmpty());
    QVERIFY(!stored->sources[1].binaryPayload.isEmpty());

    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QCOMPARE(result.sources.size(), std::size_t{2});

    // The bundled record is rebuilt from the bundled factory, not from the
    // ignored payload, and keeps its participation state and position.
    const SkyCatalogSourceRestoreEntry& restoredBundled = result.sources[0];
    QCOMPARE(restoredBundled.record.instanceId, QString("preset:bundled"));
    QCOMPARE(restoredBundled.record.title, QString("Bundled"));
    QCOMPARE(restoredBundled.record.policy, CatalogCompositionPolicy::Merge);
    QVERIFY(!restoredBundled.record.enabled);
    QVERIFY(restoredBundled.record.bundled);
    QVERIFY(restoredBundled.record.catalog != nullptr);
    QVERIFY(!restoredBundled.record.catalog->bodies().empty());
    // The rebuilt bundled catalog does not report its own deep-sky objects:
    // the live add path leaves the count empty and lets the bundled fallback
    // participation supply it, so the restored record must match.
    QCOMPARE(restoredBundled.record.foundObjectCount, std::size_t{0});
    QCOMPARE(restoredBundled.instance.instanceId, QString("preset:bundled"));
    QCOMPARE(restoredBundled.instance.descriptorId, QString("bundled"));
    QVERIFY(restoredBundled.payload.isEmpty());

    // The downloaded sibling keeps its own catalog and payload.
    const SkyCatalogSourceRestoreEntry& restoredDownloaded = result.sources[1];
    QCOMPARE(restoredDownloaded.record.instanceId, QString("custom:stars"));
    QVERIFY(!restoredDownloaded.record.bundled);
    QVERIFY(restoredDownloaded.record.catalog != nullptr);
    QCOMPARE(firstBodyId(*restoredDownloaded.record.catalog), QStringLiteral("hip_43"));

    // A configured record without any payload stays in the collection with its
    // identity, order, policy, and enabled state instead of being dropped.
    SkySettingsStore::CatalogCollectionCacheSnapshot noPayload;
    noPayload.schemaVersion = kCurrentCollectionSchemaVersion;
    SkySettingsStore::CatalogSourceCacheRecord noPayloadRecord;
    noPayloadRecord.instanceId = QStringLiteral("custom:missing-payload");
    noPayloadRecord.title = QStringLiteral("Missing payload");
    noPayloadRecord.urls = QStringList{QStringLiteral("https://example.test/missing.csv")};
    noPayloadRecord.policy = CatalogCompositionPolicy::Merge;
    noPayloadRecord.enabled = true;
    noPayload.sources.push_back(std::move(noPayloadRecord));
    QVERIFY(store.saveCatalogCollectionCache(noPayload));

    QTest::ignoreMessage(
        QtWarningMsg, "Saved catalog source cache has no payload; restoring configuration only: custom:missing-payload"
    );
    const auto noPayloadResult = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(noPayloadResult.restored);
    QCOMPARE(noPayloadResult.sources.size(), std::size_t{1});
    QCOMPARE(noPayloadResult.sources[0].record.instanceId, QString("custom:missing-payload"));
    QCOMPARE(noPayloadResult.sources[0].record.title, QString("Missing payload"));
    QCOMPARE(noPayloadResult.sources[0].record.policy, CatalogCompositionPolicy::Merge);
    QVERIFY(noPayloadResult.sources[0].record.enabled);
    QVERIFY(noPayloadResult.sources[0].record.catalog == nullptr);
    QCOMPARE(noPayloadResult.sources[0].instance.urls, QStringList{QStringLiteral("https://example.test/missing.csv")});
}

void SkyCatalogCacheControllerTests::olderRecordFormatRestoresDefinedDefaults()
{
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = kCurrentCollectionSchemaVersion - 1;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    // Older records persisted the member selection but no schema hint. The
    // descriptor identity matches the OpenNGC preset while the selected member
    // is the HYG member, so resolving the current descriptor for this record
    // would contradict the cached payload.
    SkySettingsStore::CatalogSourceCacheRecord selected = makeArchiveSourceRecord(QStringLiteral("custom:selected"));
    selected.descriptorId = QStringLiteral("open_ngc");
    selected.archiveSelector = kStarsMember;
    selected.order = 0;
    snapshot.sources.push_back(std::move(selected));

    SkySettingsStore::CatalogSourceCacheRecord ambiguous = makeArchiveSourceRecord(QStringLiteral("custom:ambiguous"));
    ambiguous.descriptorId = QStringLiteral("open_ngc");
    ambiguous.order = 1;
    snapshot.sources.push_back(std::move(ambiguous));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));
    QCOMPARE(removePersistedParseOptions().size(), 2);

    const SkyCatalogCacheController controller(&store);
    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload contains multiple supported catalog members."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; restoring configuration without payload: ZIP catalog payload "
        "contains multiple supported catalog members."
    );
    const auto result = controller.restoreCollection(0, 0, QString(), QString());

    // The stored member selection is still honored, while the missing schema
    // hint keeps its default instead of being resolved from a descriptor. The
    // record that cannot be parsed keeps its configuration without a catalog.
    QVERIFY(result.restored);
    QVERIFY(result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{2});
    const SkyCatalogSourceRestoreEntry& restored = result.sources[0];
    QCOMPARE(restored.instance.instanceId, QString("custom:selected"));
    QCOMPARE(restored.instance.archiveSelector, kStarsMember);
    QCOMPARE(restored.instance.schemaHint, CatalogSourceType::Unknown);
    QVERIFY(restored.instance.attribution.isEmpty());
    QVERIFY(restored.record.catalog != nullptr);
    QCOMPARE(restored.record.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(firstBodyId(*restored.record.catalog), QStringLiteral("hip_42"));
    QCOMPARE(result.sources[1].record.instanceId, QString("custom:ambiguous"));
    QVERIFY(result.sources[1].record.catalog == nullptr);

    // Rewriting the upgraded records records the defaults once and ends the
    // migration boundary, mirroring the manager's persist after restore.
    controller.persistCollection(persistRequestFromRestoreResult(result));
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.has_value());
    QCOMPARE(upgraded->schemaVersion, kCurrentCollectionSchemaVersion);
    QCOMPARE(upgraded->sources.size(), 2);
    QCOMPARE(upgraded->sources[0].schemaHint, CatalogSourceType::Unknown);
    QVERIFY(upgraded->sources[0].attribution.isEmpty());

    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload contains multiple supported catalog members."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Saved catalog source cache unreadable; restoring configuration without payload: ZIP catalog payload "
        "contains multiple supported catalog members."
    );
    const auto secondResult = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(secondResult.restored);
    QVERIFY(!secondResult.requiresRecordUpgrade);
    QCOMPARE(secondResult.sources.size(), std::size_t{2});
    QVERIFY(secondResult.sources[0].record.catalog != nullptr);
    QCOMPARE(firstBodyId(*secondResult.sources[0].record.catalog), QStringLiteral("hip_42"));
    QVERIFY(secondResult.sources[1].record.catalog == nullptr);
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
        const skygate::ui::internal::SkyCatalogSourceInstance customInstance =
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(customUrl);
        QCOMPARE(
            result.sources[0].record.instanceId,
            skygate::ui::internal::SkyCatalogSourceInstance::migratedLegacyInstanceId(customInstance)
        );
        QCOMPARE(result.sources[0].record.title, QString("Custom (saved)"));
        QVERIFY(result.sources[0].record.catalog != nullptr);
        QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
        QCOMPARE(result.sources[1].record.title, QString("OpenNGC (saved)"));
        QCOMPARE(result.sources[0].record.constellationData.lineRefs().size(), 1U);
    }

    // Bundled star + OpenNGC deep-sky keeps both the materialized bundled
    // star slot and the deep-sky selection.
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
        QCOMPARE(result.sources.size(), std::size_t{2});
        QCOMPARE(result.sources[0].record.instanceId, QString("preset:bundled"));
        QVERIFY(result.sources[0].record.bundled);
        QCOMPARE(result.sources[0].record.policy, CatalogCompositionPolicy::Merge);
        QVERIFY(result.sources[0].record.enabled);
        QVERIFY(result.sources[0].record.catalog != nullptr);
        QCOMPARE(result.sources[0].record.title, QString("Bundled"));
        QCOMPARE(result.sources[1].record.instanceId, QString("preset:open_ngc"));
        QVERIFY(!result.sources[1].record.bundled);
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

void SkyCatalogCacheControllerTests::emptyCollectionAfterMigrationDoesNotRestoreLegacySources()
{
    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    const QString customUrl = QStringLiteral("https://example.test/custom-stars.csv");

    // Baseline: without any stored configuration nothing is restored.
    m_settings.setCatalogCachePaths(
        m_settings.filePath(QStringLiteral("missing-legacy-star-cache.txt")),
        m_settings.filePath(QStringLiteral("missing-legacy-deep-sky-cache.txt"))
    );
    const auto nothingStored = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(!nothingStored.restored);
    QVERIFY(!nothingStored.migratedLegacy);
    QVERIFY(nothingStored.sources.empty());
    m_settings.resetCatalogCachePaths();

    const SkySettingsStore::CatalogCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
    QVERIFY(store.saveCatalogCache(legacy));

    const auto migrated = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(migrated.migratedLegacy);
    QCOMPARE(migrated.sources.size(), std::size_t{2});

    // Commit the migrated configuration, then remove every configured source.
    controller.persistCollection(persistRequestFromRestoreResult(migrated));
    controller.persistCollection(SkyCatalogCollectionPersistRequest{});

    // The empty collection is stored explicitly, not cleared: the legacy data
    // stays readable but is no longer a fallback.
    const auto storedEmpty = store.loadCatalogCollectionCache();
    QVERIFY(storedEmpty.has_value());
    QVERIFY(storedEmpty->sources.isEmpty());
    QVERIFY(store.loadCatalogCache().has_value());

    // Neither restart resurrects a removed legacy source.
    const auto firstRestart = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(firstRestart.restored);
    QVERIFY(!firstRestart.migratedLegacy);
    QVERIFY(firstRestart.sources.empty());

    const auto secondRestart = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(secondRestart.restored);
    QVERIFY(!secondRestart.migratedLegacy);
    QVERIFY(secondRestart.sources.empty());
}

void SkyCatalogCacheControllerTests::clearingPayloadCacheDoesNotReAddLegacySources()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeCollectionSnapshot()));
    // The pre-migration two-slot cache is still on disk.
    QVERIFY(store.saveCatalogCache(
        skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Legacy custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
        )
    ));

    const SkyCatalogCacheController controller(&store);
    // Clearing each source's payload cache removes its disposable payload and
    // record. It is not a configuration reset and must not retire the
    // committed configuration boundary.
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:hyg_v42")));
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:open_ngc")));
    const auto afterClear = store.loadCatalogCollectionCache();
    QVERIFY(afterClear.has_value());
    QVERIFY(afterClear->sources.isEmpty());

    controller.persistCollection(SkyCatalogCollectionPersistRequest{});

    // No legacy source is re-added (and none is re-enabled) after the caches
    // were cleared and the empty collection was persisted.
    const auto restart =
        controller.restoreCollection(2, 1, QStringLiteral("https://example.test/legacy.csv"), QString());
    QVERIFY(restart.restored);
    QVERIFY(!restart.migratedLegacy);
    QVERIFY(restart.sources.empty());
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogCacheControllerTests::failedMigrationCommitKeepsLegacyCacheReadable()
{
    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
    QVERIFY(store.saveCatalogCache(legacy));

    // Block collection payload writes so committing the migrated records fails.
    const QString blockerPath = m_settings.filePath(QStringLiteral("migration-blocker"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    blocker.write("x");
    blocker.close();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), blockerPath + QStringLiteral("/nested/cache")
    );

    const SkyCatalogCacheController controller(&store);
    const QString customUrl = QStringLiteral("https://example.test/custom-stars.csv");
    const auto migrated = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(migrated.migratedLegacy);
    QCOMPARE(migrated.sources.size(), std::size_t{2});

    // The failed commit is not recorded as a completed migration and leaves the
    // legacy cache readable.
    controller.persistCollection(persistRequestFromRestoreResult(migrated));
    QVERIFY(!store.loadCatalogCollectionCache().has_value());
    const auto legacyAfterFailure = store.loadCatalogCache();
    QVERIFY(legacyAfterFailure.has_value());
    QCOMPARE(legacyAfterFailure->sourceLabel, legacy.sourceLabel);
    QCOMPARE(legacyAfterFailure->catalogPayload, legacy.catalogPayload);
    QCOMPARE(legacyAfterFailure->deepSkyCatalogPayload, legacy.deepSkyCatalogPayload);

    // The next start migrates the same readable legacy configuration again.
    const auto retried = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(retried.migratedLegacy);
    QCOMPARE(retried.sources.size(), std::size_t{2});
    QCOMPARE(retried.sources[0].record.instanceId, migrated.sources[0].record.instanceId);
    QCOMPARE(retried.sources[1].record.instanceId, migrated.sources[1].record.instanceId);
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
