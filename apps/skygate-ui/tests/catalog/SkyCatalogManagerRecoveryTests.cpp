#include "CatalogBodyQuery.hpp"
#include "CatalogCacheTestSupport.hpp"
#include "CatalogTestPayloadFile.hpp"
#include "CatalogTestPayloads.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>
#include <utility>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogTestPayloadFile;

class SkyCatalogManagerRecoveryTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresLegacyPersistedInstanceIdsWithReferences();
    void legacyMigrationRenamesDuplicateInstanceIds();
    void legacyBundledStarSlotSurvivesMigration();
    void unreadablePayloadKeepsConfiguredAndSiblings();
    void legacySourcesStayRetiredAfterEmptyMigration();
    void interruptedGenerationUpgradeRestoresFlatSources();
    void unreadableCommittedKeepsActiveCollection();

private:
    SkySettingsStore::CatalogCacheSnapshot makeCacheSnapshot() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerRecoveryTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerRecoveryTests")));
}

void SkyCatalogManagerRecoveryTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogManagerRecoveryTests::makeCacheSnapshot() const
{
    return skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
}

void SkyCatalogManagerRecoveryTests::restoresLegacyPersistedInstanceIdsWithReferences()
{
    const QString legacyUrl = QStringLiteral("https://example.test/legacy-stars.csv");
    const skygate::ui::internal::SkyCatalogSourceInstance legacyInstance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(legacyUrl);
    const QString legacyInstanceId =
        skygate::ui::internal::SkyCatalogSourceInstance::migratedLegacyInstanceId(legacyInstance);
    QVERIFY(legacyInstanceId.startsWith(QStringLiteral("custom:")));

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = 1;
    SkySettingsStore::CatalogSourceCacheRecord record;
    record.instanceId = legacyInstanceId;
    record.title = QStringLiteral("Legacy Stars");
    record.version = QStringLiteral("v1");
    record.urls = QStringList{legacyUrl};
    record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    record.enabled = true;
    record.order = 0;
    record.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 904001, .hip = 904001, .properName = "Legacy Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(record));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        legacyUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 904002, .hip = 904002, .properName = "Reloaded Legacy Star", .mag = "1.0"}
         )}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(manager.restoreCatalogCache());

    // The legacy ID is adopted verbatim so settings groups, payload sidecars,
    // operations, and provenance keep addressing the same instance.
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));

    QStringList composedSourceIds;
    for (const QString& sourceId : manager.sourceIds()) {
        composedSourceIds.push_back(sourceId);
    }
    QVERIFY(composedSourceIds.contains(legacyInstanceId));
    bool hasLegacyContributor = false;
    for (const QStringList& contributors : manager.contributorSourceIds()) {
        hasLegacyContributor = hasLegacyContributor || contributors.contains(legacyInstanceId);
    }
    QVERIFY(hasLegacyContributor);

    // Operation lookup and reload keep referring to the restored instance ID.
    manager.retrySource(legacyInstanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Legacy Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));

    // Restart keeps the same legacy ID.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({legacyInstanceId}));
}

void SkyCatalogManagerRecoveryTests::legacyMigrationRenamesDuplicateInstanceIds()
{
    const QString sharedUrl = QStringLiteral("https://example.test/shared-slot-catalog.csv");
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Stars");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 905001, .hip = 905001, .properName = "Legacy Slot Star", .mag = "1.0"}
    );
    legacy.deepSkySourceLabel = QStringLiteral("Legacy Deep Sky");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0995",
         .type = "G",
         .ra = "00:40:00.00",
         .dec = "+12:00:00.0",
         .messier = "",
         .ngc = "0995",
         .identifiers = "PGC 9995",
         .commonName = "Legacy Slot Galaxy"}
    );

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    SkyCatalogManager manager(&store);
    manager.setCatalogPresetIndex(2);
    manager.setCatalogUrlText(sharedUrl);
    manager.setDeepSkyCatalogPresetIndex(2);
    manager.setDeepSkyCatalogUrlText(sharedUrl);

    // Both legacy slots name the same URL, so the legacy derivation maps them
    // to one ID. The second record is deterministically renamed instead of
    // overwriting the first or failing the restore.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Duplicate persisted catalog source instance id"));
    QVERIFY(manager.restoreCatalogCache());

    const QStringList restoredIds = manager.sourceInstanceIds();
    QCOMPARE(restoredIds.size(), 2);
    QVERIFY(restoredIds[0].startsWith(QStringLiteral("custom:")));
    QCOMPARE(restoredIds[1], restoredIds[0] + QStringLiteral("#2"));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Slot Star")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("NGC 995")));

    // The renamed IDs stay stable through the migration persist and restart.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), restoredIds);
}

void SkyCatalogManagerRecoveryTests::legacyBundledStarSlotSurvivesMigration()
{
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.deepSkySourceLabel = QStringLiteral("OpenNGC");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload();

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    // The legacy star slot selected the bundled source. Migration keeps it in
    // first position instead of dropping it once the downloaded deep-sky
    // source is restored.
    SkyCatalogManager manager(&store);
    manager.setDeepSkyCatalogPresetIndex(1);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(
        manager.sourceInstanceIds(), QStringList({QStringLiteral("preset:bundled"), QStringLiteral("preset:open_ngc")})
    );
    QVERIFY(manager.isSourceEnabled(QStringLiteral("preset:bundled")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    QCOMPARE(view.size(), 2);
    QCOMPARE(view[0].instanceId, QStringLiteral("preset:bundled"));
    QVERIFY(view[0].bundled);
    QCOMPARE(view[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(view[1].instanceId, QStringLiteral("preset:open_ngc"));
    QVERIFY(!view[1].bundled);
    QCOMPARE(view[1].policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // The migrated collection is durable: the next start reads the collection
    // snapshot instead of migrating the legacy cache again.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("preset:bundled"), QStringLiteral("preset:open_ngc")})
    );
}

void SkyCatalogManagerRecoveryTests::unreadablePayloadKeepsConfiguredAndSiblings()
{
    const QString damagedUrl = QStringLiteral("https://example.test/damaged-stars.csv");
    const QString healthyUrl = QStringLiteral("https://example.test/healthy-stars.csv");

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

    SkySettingsStore::CatalogSourceCacheRecord bundled;
    bundled.instanceId = QStringLiteral("primary");
    bundled.title = QStringLiteral("Bundled");
    bundled.bundled = true;
    bundled.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bundled.enabled = true;
    bundled.order = 0;
    snapshot.sources.push_back(std::move(bundled));

    SkySettingsStore::CatalogSourceCacheRecord damaged;
    damaged.instanceId = QStringLiteral("custom:damaged");
    damaged.title = QStringLiteral("Damaged Source");
    damaged.urls = QStringList{damagedUrl};
    damaged.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    damaged.enabled = true;
    damaged.order = 1;
    damaged.payload = "this is not a catalog";
    snapshot.sources.push_back(std::move(damaged));

    SkySettingsStore::CatalogSourceCacheRecord healthy;
    healthy.instanceId = QStringLiteral("custom:healthy");
    healthy.title = QStringLiteral("Healthy Source");
    healthy.urls = QStringList{healthyUrl};
    healthy.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    healthy.enabled = true;
    healthy.order = 2;
    healthy.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 906010, .hip = 906010, .properName = "Healthy Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(healthy));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        damagedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 906011, .hip = 906011, .properName = "Recovered Star", .mag = "1.0"}
         )}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Saved catalog source cache unreadable; restoring configuration without payload")
    );
    QVERIFY(manager.restoreCatalogCache());

    // The damaged source keeps its identity, order, policy, and participation
    // state next to its successfully restored siblings.
    QCOMPARE(
        manager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(manager.isSourceEnabled(QStringLiteral("custom:damaged")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Unavailable sources")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Healthy Star")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto damagedEntry =
        std::find_if(view.begin(), view.end(), [](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == QStringLiteral("custom:damaged");
        });
    QVERIFY(damagedEntry != view.end());
    QVERIFY(damagedEntry->hasError);
    QCOMPARE(damagedEntry->statusText, QStringLiteral("Payload unavailable"));
    QCOMPARE(damagedEntry->objectCount, std::size_t{0});

    // The upgrade persist keeps the unreadable record and its raw payload for
    // diagnostics instead of erasing the configured source.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QCOMPARE(persisted.snapshot.sources.size(), 3);
    const auto persistedDamaged = std::find_if(
        persisted.snapshot.sources.begin(),
        persisted.snapshot.sources.end(),
        [](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == QStringLiteral("custom:damaged");
        }
    );
    QVERIFY(persistedDamaged != persisted.snapshot.sources.end());
    QCOMPARE(persistedDamaged->urls, QStringList{damagedUrl});
    QCOMPARE(persistedDamaged->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(persistedDamaged->enabled);
    QCOMPARE(persistedDamaged->payload, QByteArray("this is not a catalog"));

    // The preserved configuration is enough to retry the configured download.
    manager.retrySource(QStringLiteral("custom:damaged"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Recovered Star")));

    // A further restart keeps all three configured sources.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Recovered Star")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Healthy Star")));
}

void SkyCatalogManagerRecoveryTests::legacySourcesStayRetiredAfterEmptyMigration()
{
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Custom");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Legacy Star", .mag = "1.0"}
    );
    legacy.deepSkySourceLabel = QStringLiteral("Legacy OpenNGC");
    legacy.deepSkyCatalogPayload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload(
        {.name = "NGC0707", .messier = "", .ngc = "0707", .identifiers = "PGC 707", .commonName = "Legacy Galaxy"}
    );

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));

    {
        SkyCatalogManager manager(&store);
        manager.setCatalogPresetIndex(2);
        manager.setDeepSkyCatalogPresetIndex(2);
        QVERIFY(manager.restoreCatalogCache());

        QCOMPARE(manager.sourceCount(), std::size_t{2});
        QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
        QCOMPARE(manager.sourceViewEntries().size(), 2);
        QVERIFY(manager.statusText().contains(QStringLiteral("Legacy")));

        // Removing every migrated source commits the collection boundary; the
        // legacy cache itself stays readable instead of being deleted. The
        // last removal leaves the collection deliberately empty instead of
        // reinstating an implicit default source.
        const QStringList migratedInstanceIds = manager.sourceInstanceIds();
        for (const QString& instanceId : migratedInstanceIds) {
            manager.removeSource(instanceId);
        }
        QVERIFY(manager.sourceInstanceIds().isEmpty());
        QVERIFY(manager.sourceViewEntries().isEmpty());
        QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
        QVERIFY(store.loadCatalogCache().has_value());
        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // The restart reads the committed empty collection instead of resurrecting
    // the still-readable legacy two-slot cache, and the bundled default source
    // is not reinstated.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(restoredManager.sourceInstanceIds().isEmpty());
    QVERIFY(restoredManager.sourceViewEntries().isEmpty());
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restoredManager.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!restoredManager.sourceTitles().values().contains(QStringLiteral("Legacy Custom")));
    QVERIFY(!restoredManager.sourceTitles().values().contains(QStringLiteral("Legacy OpenNGC")));
    QVERIFY(restoredManager.participationSummary().startsWith(QStringLiteral("Bundled")));
    QVERIFY(store.loadCatalogCache().has_value());

    // An explicitly empty persisted collection - what remains after every
    // source of a migrated collection was removed - is a committed
    // configuration as well: the restart restores it as an empty collection of
    // zero sources and never falls back to the legacy two-slot cache that is
    // still readable.
    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    SkyCatalogManager emptyCollectionManager(&store);
    QVERIFY(emptyCollectionManager.restoreCatalogCache());
    QVERIFY(emptyCollectionManager.sourceInstanceIds().isEmpty());
    QVERIFY(emptyCollectionManager.sourceViewEntries().isEmpty());
    QVERIFY(
        !CatalogBodyQuery::containsDisplayName(emptyCollectionManager.starCatalog(), QStringLiteral("Legacy Star"))
    );
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy Custom")));
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy OpenNGC")));
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerRecoveryTests::interruptedGenerationUpgradeRestoresFlatSources()
{
    SkySettingsStore store;
    // The retired two-slot cache stays readable on disk, so a restore that
    // migrated it instead of the flat collection would return its source.
    QVERIFY(store.saveCatalogCache(makeCacheSnapshot()));

    const QString directory = m_settings.filePath(QStringLiteral("catalog-collection-cache"));
    QVERIFY(QDir().mkpath(directory));

    const QString firstPayloadPath = QDir(directory).filePath(QStringLiteral("flat-stars.txt"));
    const QString secondPayloadPath = QDir(directory).filePath(QStringLiteral("flat-deep-sky.txt"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            firstPayloadPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 909101, .hip = 909101, .properName = "Flat Star", .mag = "1.0"}
            )
        )
    );
    QVERIFY(CatalogTestPayloadFile::write(secondPayloadPath, skygate::ui::tests::sampleCompactOpenNgcCsvPayload()));

    // A flat collection written before generations were published: its
    // identities, order, and options are the committed configuration.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-stars"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:stars"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_stars"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat Stars"));
        settings.setValue(QStringLiteral("version"), QStringLiteral("v1"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-stars.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge)
        );
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 0);
        settings.setValue(QStringLiteral("payloadPath"), firstPayloadPath);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-dso"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:dso"));
        settings.setValue(QStringLiteral("descriptorId"), QStringLiteral("flat_dso"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat DSO"));
        settings.setValue(QStringLiteral("version"), QStringLiteral("v1"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-dso.csv")});
        settings.setValue(
            QStringLiteral("policy"), static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly)
        );
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 1);
        settings.setValue(QStringLiteral("payloadPath"), secondPayloadPath);
        settings.endGroup();
        settings.sync();
    }

    // An interrupted first upgrade staged a generation record without
    // publishing a manifest.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
        settings.beginGroup(QStringLiteral("catalogSources/4/catalog-source-staged"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("staged:source"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Source"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.endGroup();
        settings.sync();
    }

    // The flat records are the committed configuration, so the manager
    // restores them instead of the readable two-slot cache and upgrades them
    // to the current format.
    {
        SkyCatalogManager manager(&store);
        QVERIFY(manager.restoreCatalogCache());
        QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("flat:stars"), QStringLiteral("flat:dso")}));
        QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Flat Star")));
        QVERIFY(!manager.sourceTitles().values().contains(QStringLiteral("Custom")));
    }

    // The upgrade committed a generation with the same identities, options,
    // and order; the staged record is gone rather than promoted.
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.isLoaded());
    QCOMPARE(
        upgraded.snapshot.schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    QCOMPARE(upgraded.snapshot.sources.size(), 2);
    QCOMPARE(upgraded.snapshot.sources[0].instanceId, QStringLiteral("flat:stars"));
    QCOMPARE(upgraded.snapshot.sources[0].descriptorId, QStringLiteral("flat_stars"));
    QCOMPARE(upgraded.snapshot.sources[0].urls, QStringList{QStringLiteral("https://example.test/flat-stars.csv")});
    QCOMPARE(upgraded.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(upgraded.snapshot.sources[0].enabled);
    QCOMPARE(upgraded.snapshot.sources[0].order, 0);
    QCOMPARE(upgraded.snapshot.sources[1].instanceId, QStringLiteral("flat:dso"));
    QCOMPARE(upgraded.snapshot.sources[1].urls, QStringList{QStringLiteral("https://example.test/flat-dso.csv")});
    QCOMPARE(upgraded.snapshot.sources[1].policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QCOMPARE(upgraded.snapshot.sources[1].order, 1);

    // Two restarts restore the upgraded generation with the same identities
    // and order; the retired two-slot source never returns.
    for (int restart = 0; restart < 2; ++restart) {
        SkyCatalogManager restarted(&store);
        QVERIFY(restarted.restoreCatalogCache());
        QCOMPARE(
            restarted.sourceInstanceIds(), QStringList({QStringLiteral("flat:stars"), QStringLiteral("flat:dso")})
        );
        QVERIFY(CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Flat Star")));
        QVERIFY(!restarted.sourceTitles().values().contains(QStringLiteral("Custom")));
    }
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerRecoveryTests::unreadableCommittedKeepsActiveCollection()
{
    // The retired pre-generation two-slot cache stays readable on disk, so a
    // restore that fell back to it would visibly resurrect its source.
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Custom");
    legacy.catalogPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908001, .hip = 908001, .properName = "Legacy Star", .mag = "1.0"}
    );

    SkySettingsStore::CatalogCollectionCacheSnapshot committed;
    committed.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    committed.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    SkySettingsStore::CatalogSourceCacheRecord accepted;
    accepted.instanceId = QStringLiteral("custom:accepted");
    accepted.title = QStringLiteral("Accepted Source");
    accepted.urls = QStringList{QStringLiteral("https://example.test/accepted-stars.csv")};
    accepted.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    accepted.enabled = true;
    accepted.order = 0;
    accepted.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908101, .hip = 908101, .properName = "Accepted Star", .mag = "1.0"}
    );
    committed.sources.push_back(std::move(accepted));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(legacy));
    QVERIFY(store.saveCatalogCollectionCache(committed));

    SkyCatalogManager manager(&store);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("custom:accepted")});
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));

    // The committed manifest is lost while its records and the commit record
    // survive, so the readable legacy cache must not be migrated over the
    // committed configuration.
    const QString collectionCacheDirectory = m_settings.filePath(QStringLiteral("catalog-collection-cache"));
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(collectionCacheDirectory)));

    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    const std::size_t bodyCountBefore = manager.bodyCount();
    const std::uint64_t revisionBefore = manager.catalogRevision();

    QVERIFY(!manager.restoreCatalogCache());

    // The accepted collection stays active: no legacy source returns and no
    // catalog change is published for the rejected restore.
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("custom:accepted")});
    QCOMPARE(manager.bodyCount(), bodyCountBefore);
    QCOMPARE(manager.catalogRevision(), revisionBefore);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
    QCOMPARE(catalogSpy.count(), 0);

    // The failure is reported once through the normal status signal, naming
    // the unreadable configuration and the recovery action.
    QCOMPARE(statusSpy.count(), 1);
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Saved catalog configuration unreadable")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Add, update, or reorder a catalog source")));
    QVERIFY(manager.statusText().contains(QStringLiteral("recorded as committed")));

    // A later start reaches the same decision: the rejected restore left the
    // stored state and the readable legacy cache untouched, and neither of
    // them replaces the first-use default.
    SkyCatalogManager restarted(&store);
    QVERIFY(!restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
    QVERIFY(store.loadCatalogCache().has_value());
}

QTEST_GUILESS_MAIN(SkyCatalogManagerRecoveryTests)

#include "SkyCatalogManagerRecoveryTests.moc"
