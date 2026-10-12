#include "CatalogCacheFixture.hpp"
#include "CatalogTestPayloads.hpp"
#include "SettingsTestFixture.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::internal::SkyCatalogSourcePersistEntry;
using skygate::ui::tests::CatalogCacheFixture;

namespace {

constexpr int kCurrentCollectionSchemaVersion =
    skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;
constexpr int kCurrentConstellationSchemaVersion =
    skygate::ui::internal::SkyCatalogCollectionStore::kConstellationLineCacheSchemaVersion;

}  // namespace

class SkyCatalogCacheRelatedDataTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresCollectionSourcesAndConstellationLabels();
    void roundTripsEachSourcesOwnRelatedDataset();
    void adoptsPriorSnapshotWithOneRelatedOwner();
    void dropsPriorDuplicatedRelatedPayloads();
    void corruptRelatedPayloadLeavesSiblingsIntact();

private:
    void resetSettings();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheRelatedDataTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheRelatedDataTests")));
}

void SkyCatalogCacheRelatedDataTests::init()
{
    resetSettings();
}

void SkyCatalogCacheRelatedDataTests::resetSettings()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();
}

void SkyCatalogCacheRelatedDataTests::restoresCollectionSourcesAndConstellationLabels()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));

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

void SkyCatalogCacheRelatedDataTests::roundTripsEachSourcesOwnRelatedDataset()
{
    auto sourceACatalog = CatalogCacheFixture::makeStarCatalog("hip_11", "Owner A Star");
    auto sourceBCatalog = CatalogCacheFixture::makeStarCatalog("hip_22", "Owner B Star");

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

void SkyCatalogCacheRelatedDataTests::adoptsPriorSnapshotWithOneRelatedOwner()
{
    // A record written before the per-source related format holds a copy of
    // the composed collection-wide view. With one such record in the snapshot
    // no other record could have supplied the copy, so that record is its
    // owner and the payload is migrated instead of discarded.
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
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

void SkyCatalogCacheRelatedDataTests::dropsPriorDuplicatedRelatedPayloads()
{
    // The prior writer serialized the same composed collection-wide view into
    // every eligible source record. Identical copies in several records cannot
    // establish an owner, so none of them is attributed to a record.
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
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

void SkyCatalogCacheRelatedDataTests::corruptRelatedPayloadLeavesSiblingsIntact()
{
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
    // The star record's line payload is unreadable...
    snapshot.sources[0].constellationLineRows = "not a related line payload";
    // ... and the deep-sky record's anchor payload is unreadable.
    snapshot.sources[1].constellationLineRows = "hip_51|hip_52\n";
    snapshot.sources[1].constellationAnchorGroupRows = "not a related anchor payload";
    snapshot.sources[1].constellationLineSchemaVersion = kCurrentConstellationSchemaVersion;
    snapshot.sources[1].constellationCount = 1;

    // A third, healthy record carries a distinguishable dataset.
    SkyCatalogSourceCacheRecord healthy;
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

QTEST_GUILESS_MAIN(SkyCatalogCacheRelatedDataTests)

#include "SkyCatalogCacheRelatedDataTests.moc"
