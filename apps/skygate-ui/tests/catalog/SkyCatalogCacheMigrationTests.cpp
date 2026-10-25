#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogCacheController.hpp"
#include "catalog/SkyCatalogCollectionPersistRequest.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheFixture.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/CatalogTestPayloads.hpp"
#include <skygate/testsupport/LogCapture.hpp>
#include "support/SettingsTestFixture.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <utility>

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::tests::CatalogCacheFixture;
using skygate::ui::tests::catalogCollectionManifestFilePath;
using skygate::ui::tests::committedCatalogCollectionGeneration;
using skygate::ui::tests::sampleCatalogCacheSnapshot;
using skygate::ui::tests::stagedCatalogSourceSidecarPath;

class SkyCatalogCacheMigrationTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void clearSourceCacheVersusClearCollectionCache();
    void migratesLegacyBundledCustomMixedConfigurations();
    void repeatedMigrationIsIdempotent();
    void emptyMigratedCollectionDoesNotRestoreLegacy();
    void clearingPayloadCacheDoesNotReAddLegacySources();
    void failedMigrationCommitKeepsLegacyCacheReadable();
    void failedCollectionWritePreservesPriorData();
    void truncatedCommittedManifestRejectsLegacyMigration();
    void restoreUsesCommittedOverStagedGeneration();
    void missingManifestStagedGenerationRejectsRestore();
    void interruptedGenerationStagingKeepsFlatCollection();
    void logsCollectionLifecycleSummariesAtInfoLevel();

private:
    void resetSettings();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheMigrationTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheMigrationTests")));
}

void SkyCatalogCacheMigrationTests::init()
{
    resetSettings();
}

void SkyCatalogCacheMigrationTests::resetSettings()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();
}

void SkyCatalogCacheMigrationTests::clearSourceCacheVersusClearCollectionCache()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));

    const SkyCatalogCacheController controller(&store);
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:hyg_v42")));

    // Evicting one source's payload keeps its configured record and the peer
    // record unchanged; only the cleared payload references are dropped.
    auto remaining = store.loadCatalogCollectionCache();
    QVERIFY(remaining.isLoaded());
    QCOMPARE(remaining.snapshot.sources.size(), 2);
    QCOMPARE(remaining.snapshot.sources[0].instanceId, QString("preset:hyg_v42"));
    QCOMPARE(remaining.snapshot.sources[0].descriptorId, QString("hyg_v42"));
    QCOMPARE(remaining.snapshot.sources[0].title, QString("HYG v4.2"));
    QCOMPARE(remaining.snapshot.sources[0].urls, QStringList{QStringLiteral("https://example.test/hyg.csv.gz")});
    QCOMPARE(
        remaining.snapshot.sources[0].relatedDatasetUrls, QStringList{QStringLiteral("https://example.test/lines.json")}
    );
    QCOMPARE(remaining.snapshot.sources[0].policy, CatalogCompositionPolicy::Merge);
    QCOMPARE(remaining.snapshot.sources[0].enabled, true);
    QCOMPARE(remaining.snapshot.sources[0].order, 0);
    QVERIFY(remaining.snapshot.sources[0].payload.isEmpty());
    QVERIFY(remaining.snapshot.sources[0].binaryPayload.isEmpty());
    QVERIFY(remaining.snapshot.sources[0].constellationLineRows.isEmpty());
    QVERIFY(remaining.snapshot.sources[0].constellationAnchorGroupRows.isEmpty());
    QCOMPARE(remaining.snapshot.sources[0].constellationCount, std::size_t{0});
    QCOMPARE(remaining.snapshot.sources[1].instanceId, QString("preset:open_ngc"));
    QCOMPARE(remaining.snapshot.sources[1].payload, skygate::ui::tests::sampleCompactOpenNgcCsvPayload());

    QVERIFY(store.clearCatalogCollectionCache());
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
}

void SkyCatalogCacheMigrationTests::migratesLegacyBundledCustomMixedConfigurations()
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
        const SkyCatalogLegacyCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
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
        SkyCatalogLegacyCacheSnapshot legacy;
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

void SkyCatalogCacheMigrationTests::repeatedMigrationIsIdempotent()
{
    SkySettingsStore store;
    const SkyCatalogLegacyCacheSnapshot legacy =
        skygate::ui::tests::sampleCatalogCacheSnapshot({.sourceLabel = QStringLiteral("HYG v4.2")});
    QVERIFY(store.saveCatalogCache(legacy));

    const SkyCatalogCacheController controller(&store);
    const QString customUrl = QStringLiteral("https://example.test/hyg.csv.gz");
    const auto firstResult = controller.restoreCollection(1, 1, customUrl, QString());
    QVERIFY(firstResult.migratedLegacy);
    QCOMPARE(firstResult.sources.size(), std::size_t{2});

    // Write the migrated records back, mirroring the manager's upgrade path.
    controller.persistCollection(CatalogCacheFixture::persistRequestFromRestoreResult(firstResult));

    const auto secondResult = controller.restoreCollection(1, 1, customUrl, QString());
    QVERIFY(!secondResult.migratedLegacy);
    QCOMPARE(secondResult.sources.size(), std::size_t{2});
    QCOMPARE(secondResult.sources[0].record.instanceId, firstResult.sources[0].record.instanceId);
    QCOMPARE(secondResult.sources[1].record.instanceId, firstResult.sources[1].record.instanceId);
}

void SkyCatalogCacheMigrationTests::emptyMigratedCollectionDoesNotRestoreLegacy()
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

    const SkyCatalogLegacyCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
    QVERIFY(store.saveCatalogCache(legacy));

    const auto migrated = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(migrated.migratedLegacy);
    QCOMPARE(migrated.sources.size(), std::size_t{2});

    // Commit the migrated configuration, then remove every configured source.
    controller.persistCollection(CatalogCacheFixture::persistRequestFromRestoreResult(migrated));
    controller.persistCollection(SkyCatalogCollectionPersistRequest{});

    // The empty collection is stored explicitly, not cleared: the legacy data
    // stays readable but is no longer a fallback.
    const auto storedEmpty = store.loadCatalogCollectionCache();
    QVERIFY(storedEmpty.isLoaded());
    QVERIFY(storedEmpty.snapshot.sources.isEmpty());
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

void SkyCatalogCacheMigrationTests::clearingPayloadCacheDoesNotReAddLegacySources()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));
    // The pre-migration two-slot cache is still on disk.
    QVERIFY(store.saveCatalogCache(
        skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Legacy custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
        )
    ));

    const SkyCatalogCacheController controller(&store);
    // Clearing each source's payload evicts its disposable bytes while every
    // configured record stays. It is not a configuration reset and must not
    // retire the committed configuration boundary.
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:hyg_v42")));
    QVERIFY(controller.clearSourceCache(QStringLiteral("preset:open_ngc")));
    const auto afterClear = store.loadCatalogCollectionCache();
    QVERIFY(afterClear.isLoaded());
    QCOMPARE(afterClear.snapshot.sources.size(), 2);
    QVERIFY(afterClear.snapshot.sources[0].payload.isEmpty());
    QVERIFY(afterClear.snapshot.sources[1].payload.isEmpty());

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

void SkyCatalogCacheMigrationTests::failedMigrationCommitKeepsLegacyCacheReadable()
{
    SkySettingsStore store;
    const SkyCatalogLegacyCacheSnapshot legacy = skygate::ui::tests::sampleCatalogCacheSnapshot(
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
    controller.persistCollection(CatalogCacheFixture::persistRequestFromRestoreResult(migrated));
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
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

void SkyCatalogCacheMigrationTests::failedCollectionWritePreservesPriorData()
{
    auto oldStarCatalog = CatalogCacheFixture::makeStarCatalog("hip_11", "Old Star");
    auto oldDeepSkyCatalog = CatalogCacheFixture::makeStarCatalog("ngc_22", "Old Galaxy");

    // The committed collection and the attempted update differ in title, raw
    // bytes, and binary bytes for both sources, so a mixture would be visible.
    auto oldSnapshot = CatalogCacheFixture::makeCollectionSnapshot();
    oldSnapshot.sources[0].title = QStringLiteral("Old star title");
    oldSnapshot.sources[0].payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 11, .properName = "Old Star"});
    oldSnapshot.sources[0].binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(oldStarCatalog->catalog());
    oldSnapshot.sources[1].title = QStringLiteral("Old deep sky title");
    oldSnapshot.sources[1].payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload(
        {.name = "NGC0022", .messier = "", .ngc = "0022", .identifiers = "PGC 22", .commonName = "Old Galaxy"}
    );
    oldSnapshot.sources[1].binaryPayload =
        skygate::ephemeris::CatalogBinaryCodec::serialize(oldDeepSkyCatalog->catalog());

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldSnapshot));

    // The update's second source binary sidecar is forced to fail after the
    // first source's new files were already staged.
    auto newStarCatalog = CatalogCacheFixture::makeStarCatalog("hip_99", "New Star");
    auto newDeepSkyCatalog = CatalogCacheFixture::makeStarCatalog("ngc_99", "New Galaxy");
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("collection-cache"));
    const QString blockedBinaryPath = stagedCatalogSourceSidecarPath(
        cacheDirectory,
        committedCatalogCollectionGeneration(cacheDirectory) + 1U,
        QStringLiteral("preset:open_ngc"),
        QStringLiteral(".bin")
    );
    QVERIFY(QDir().mkpath(blockedBinaryPath));

    auto updatedSnapshot = CatalogCacheFixture::makeCollectionSnapshot();
    updatedSnapshot.sources[0].title = QStringLiteral("New star title");
    updatedSnapshot.sources[0].payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 99, .properName = "New Star"});
    updatedSnapshot.sources[0].binaryPayload =
        skygate::ephemeris::CatalogBinaryCodec::serialize(newStarCatalog->catalog());
    updatedSnapshot.sources[1].title = QStringLiteral("New deep sky title");
    updatedSnapshot.sources[1].payload = skygate::ui::tests::sampleCompactOpenNgcCsvPayload(
        {.name = "NGC0099", .messier = "", .ngc = "0099", .identifiers = "PGC 99", .commonName = "New Galaxy"}
    );
    updatedSnapshot.sources[1].binaryPayload =
        skygate::ephemeris::CatalogBinaryCodec::serialize(newDeepSkyCatalog->catalog());

    QVERIFY(!store.saveCatalogCollectionCache(updatedSnapshot));

    // Every previously committed source keeps its original metadata, raw
    // bytes, and binary bytes; nothing from the staged update is visible.
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QCOMPARE(loaded.snapshot.sources.size(), 2);
    QCOMPARE(loaded.snapshot.sources[0].title, oldSnapshot.sources[0].title);
    QCOMPARE(loaded.snapshot.sources[0].payload, oldSnapshot.sources[0].payload);
    QCOMPARE(loaded.snapshot.sources[0].binaryPayload, oldSnapshot.sources[0].binaryPayload);
    QCOMPARE(loaded.snapshot.sources[1].title, oldSnapshot.sources[1].title);
    QCOMPARE(loaded.snapshot.sources[1].payload, oldSnapshot.sources[1].payload);
    QCOMPARE(loaded.snapshot.sources[1].binaryPayload, oldSnapshot.sources[1].binaryPayload);
    QVERIFY(loaded.snapshot.sources[0].payload != updatedSnapshot.sources[0].payload);
    QVERIFY(loaded.snapshot.sources[0].binaryPayload != updatedSnapshot.sources[0].binaryPayload);

    // A restart exposes the complete old catalogs, not old metadata with new
    // objects.
    const SkyCatalogCacheController controller(&store);
    const auto restored = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(restored.restored);
    QCOMPARE(restored.sources.size(), std::size_t{2});
    QVERIFY(!restored.sources[0].requiresBinaryUpgrade);
    QCOMPARE(restored.sources[0].record.title, QString("Old star title (saved)"));
    QCOMPARE(CatalogCacheFixture::firstBodyId(*restored.sources[0].record.catalog), QStringLiteral("hip_11"));
    QCOMPARE(restored.sources[1].record.title, QStringLiteral("Old deep sky title (saved)"));
    QCOMPARE(CatalogCacheFixture::firstBodyId(*restored.sources[1].record.catalog), QStringLiteral("ngc_22"));
}

void SkyCatalogCacheMigrationTests::truncatedCommittedManifestRejectsLegacyMigration()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(
        skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Legacy Custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
        )
    ));
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));

    // The committed manifest exists but is incomplete, so the committed
    // records cannot be addressed and the stored state is unusable.
    const QString directory = m_settings.filePath(QStringLiteral("collection-cache"));
    const QString manifestPath = catalogCollectionManifestFilePath(directory);
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(manifestFile.write("schemaVersion=2\ngeneration=") > 0);
    manifestFile.close();

    const SkyCatalogCacheController controller(&store);
    const auto result =
        controller.restoreCollection(2, 1, QStringLiteral("https://example.test/legacy-stars.csv"), QString());
    QVERIFY(!result.restored);
    QVERIFY(!result.migratedLegacy);
    QVERIFY(result.unusableCommittedCollection);
    QVERIFY(result.sources.empty());
    QVERIFY(result.statusText.contains(QStringLiteral("manifest is unreadable")));
    QVERIFY(store.loadCatalogCache().has_value());

    // The committed configuration is still stored; only its manifest is
    // incomplete, so a later start reaches the same decision instead of
    // reading the legacy sources.
    QCOMPARE(store.loadCatalogCollectionCache().state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
}

void SkyCatalogCacheMigrationTests::restoreUsesCommittedOverStagedGeneration()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));

    const QString directory = m_settings.filePath(QStringLiteral("collection-cache"));
    const quint64 committedGeneration = committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // A later save staged a newer generation whose manifest was never
    // published: its records exist, but nothing names them as committed.
    const quint64 stagedGeneration = committedGeneration + 1U;
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/%1/recordsStored").arg(stagedGeneration), true);
        settings.beginGroup(QStringLiteral("catalogSources/%1/catalog-source-staged").arg(stagedGeneration));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:staged"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Title"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.endGroup();
        settings.sync();
    }

    // The committed generation stays the current collection.
    const SkyCatalogCacheController controller(&store);
    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QVERIFY(!result.migratedLegacy);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].record.instanceId, QStringLiteral("preset:hyg_v42"));
    QCOMPARE(result.sources[1].record.instanceId, QStringLiteral("preset:open_ngc"));
}

void SkyCatalogCacheMigrationTests::missingManifestStagedGenerationRejectsRestore()
{
    SkySettingsStore store;
    // The retired pre-generation two-slot cache stays readable on disk for the
    // whole test, so a restore that fell back to it would return its sources.
    QVERIFY(store.saveCatalogCache(
        skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Legacy Custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
        )
    ));

    const SkyCatalogCacheController controller(&store);
    const QString customUrl = QStringLiteral("https://example.test/legacy-stars.csv");

    // Migrate the legacy slots once and then commit an intentionally empty
    // collection over them: the current configuration holds no source, while
    // the legacy data it replaced stays readable.
    const auto migrated = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(migrated.migratedLegacy);
    QCOMPARE(migrated.sources.size(), std::size_t{2});
    controller.persistCollection(CatalogCacheFixture::persistRequestFromRestoreResult(migrated));
    controller.persistCollection(SkyCatalogCollectionPersistRequest{});
    const auto committed = store.loadCatalogCollectionCache();
    QVERIFY(committed.isLoaded());
    QVERIFY(committed.snapshot.sources.isEmpty());

    const QString directory = m_settings.filePath(QStringLiteral("collection-cache"));
    const QString manifestPath = catalogCollectionManifestFilePath(directory);
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QByteArray committedManifest = manifestFile.readAll();
    manifestFile.close();
    const quint64 committedGeneration = committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // A later save staged a newer generation whose manifest was never
    // published: its settings record and sidecar exist, but nothing names that
    // generation as committed.
    const quint64 stagedGeneration = committedGeneration + 1U;
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/%1/recordsStored").arg(stagedGeneration), true);
        settings.beginGroup(QStringLiteral("catalogSources/%1/catalog-source-staged").arg(stagedGeneration));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:staged"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Title"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.endGroup();
        settings.sync();
    }
    const QString stagedSidecarPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, stagedGeneration, QStringLiteral("custom:staged"), QStringLiteral(".txt")
    );
    {
        QFile stagedSidecar(stagedSidecarPath);
        QVERIFY(stagedSidecar.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(stagedSidecar.write("staged raw payload"), qint64(18));
    }

    // Only the committed manifest is lost. Neither the readable legacy cache
    // nor the staged newer generation proves that it is the current
    // collection, so the committed configuration still owns it.
    QVERIFY(QFile::remove(manifestPath));

    const auto unreadable = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(!unreadable.restored);
    QVERIFY(!unreadable.migratedLegacy);
    QVERIFY(unreadable.unusableCommittedCollection);
    QVERIFY(unreadable.sources.empty());
    QVERIFY(unreadable.statusText.startsWith(QStringLiteral("Catalog: Saved catalog configuration unreadable")));
    QVERIFY(unreadable.statusText.contains(QStringLiteral("recorded as committed")));

    // Repeated starts reach the same decision, and the read paths leave every
    // discarded candidate untouched: the legacy cache stays readable and the
    // staged sidecar keeps its bytes.
    const auto repeated = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(!repeated.restored);
    QVERIFY(!repeated.migratedLegacy);
    QVERIFY(repeated.unusableCommittedCollection);
    QVERIFY(repeated.sources.empty());
    QVERIFY(store.loadCatalogCache().has_value());
    QCOMPARE(store.loadCatalogCollectionCache().state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    {
        QFile stagedSidecar(stagedSidecarPath);
        QVERIFY(stagedSidecar.open(QIODevice::ReadOnly));
        QCOMPARE(stagedSidecar.readAll(), QByteArray("staged raw payload"));
    }

    // The committed configuration is still the recovery point, so restoring
    // its manifest brings back the intentionally empty collection instead of
    // the staged generation or the readable legacy sources.
    {
        QFile restoredManifest(manifestPath);
        QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(restoredManifest.write(committedManifest), qint64(committedManifest.size()));
    }
    const auto recovered = controller.restoreCollection(2, 1, customUrl, QString());
    QVERIFY(recovered.restored);
    QVERIFY(!recovered.migratedLegacy);
    QVERIFY(recovered.sources.empty());
}

void SkyCatalogCacheMigrationTests::interruptedGenerationStagingKeepsFlatCollection()
{
    SkySettingsStore store;
    // The retired pre-generation two-slot cache stays readable on disk, so a
    // restore that fell back to it would return its legacy source.
    QVERIFY(store.saveCatalogCache(
        skygate::ui::tests::sampleCatalogCacheSnapshot(
            {.sourceLabel = QStringLiteral("Legacy Custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
        )
    ));

    const QString directory = m_settings.filePath(QStringLiteral("collection-cache"));
    QVERIFY(QDir().mkpath(directory));

    const QString firstPayloadPath = QDir(directory).filePath(QStringLiteral("flat-first.txt"));
    const QString secondPayloadPath = QDir(directory).filePath(QStringLiteral("flat-second.txt"));
    const auto writeFile = [](const QString& path, const QByteArray& contents) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        return file.write(contents) == contents.size();
    };
    QVERIFY(writeFile(
        firstPayloadPath,
        skygate::ui::tests::sampleHygCsvPayload({.id = 909001, .hip = 909001, .properName = "Flat Star", .mag = "1.0"})
    ));
    QVERIFY(writeFile(secondPayloadPath, skygate::ui::tests::sampleCompactOpenNgcCsvPayload()));

    // A flat collection written before generations were published: two
    // configured sources whose order, identities, and options must survive.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-stars"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:stars"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat Stars"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-stars.csv")});
        settings.setValue(QStringLiteral("policy"), static_cast<int>(CatalogCompositionPolicy::Merge));
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 0);
        settings.setValue(QStringLiteral("payloadPath"), firstPayloadPath);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-flat-dso"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("flat:dso"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Flat DSO"));
        settings.setValue(QStringLiteral("urls"), QStringList{QStringLiteral("https://example.test/flat-dso.csv")});
        settings.setValue(QStringLiteral("policy"), static_cast<int>(CatalogCompositionPolicy::DeepSkyOnly));
        settings.setValue(QStringLiteral("enabled"), true);
        settings.setValue(QStringLiteral("order"), 1);
        settings.setValue(QStringLiteral("payloadPath"), secondPayloadPath);
        settings.endGroup();
        settings.sync();
    }

    // An interrupted first upgrade staged a generation record and sidecar
    // without publishing a manifest.
    const quint64 stagedGeneration = 4U;
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/%1/recordsStored").arg(stagedGeneration), true);
        settings.beginGroup(QStringLiteral("catalogSources/%1/catalog-source-staged").arg(stagedGeneration));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("staged:source"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Staged Title"));
        settings.setValue(QStringLiteral("order"), 0);
        settings.endGroup();
        settings.sync();
    }
    const QString stagedSidecarPath = stagedCatalogSourceSidecarPath(
        directory, stagedGeneration, QStringLiteral("staged:source"), QStringLiteral(".txt")
    );
    QVERIFY(writeFile(stagedSidecarPath, QByteArray("staged raw payload")));

    const SkyCatalogCacheController controller(&store);
    const auto result =
        controller.restoreCollection(2, 1, QStringLiteral("https://example.test/legacy-stars.csv"), QString());

    // The flat collection is already committed configuration, so it is
    // restored instead of the readable legacy two-slot cache and with no
    // legacy migration, while the staged generation stays ignored.
    QVERIFY(result.restored);
    QVERIFY(!result.migratedLegacy);
    QVERIFY(result.requiresRecordUpgrade);
    QCOMPARE(result.sources.size(), std::size_t{2});
    QCOMPARE(result.sources[0].instance.instanceId, QStringLiteral("flat:stars"));
    QCOMPARE(result.sources[0].instance.title, QStringLiteral("Flat Stars"));
    QCOMPARE(result.sources[0].record.policy, CatalogCompositionPolicy::Merge);
    QCOMPARE(result.sources[1].instance.instanceId, QStringLiteral("flat:dso"));
    QCOMPARE(result.sources[1].instance.title, QStringLiteral("Flat DSO"));
    QCOMPARE(result.sources[1].record.policy, CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(result.sources[0].record.catalog != nullptr);
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[0].record.catalog), QStringLiteral("hip_909001"));
    QVERIFY(result.sources[1].record.catalog != nullptr);
    QCOMPARE(result.sources[1].record.catalog->bodies().size(), std::size_t{1});

    // The readable legacy cache and the staged sidecar are left untouched by
    // the restore that used the flat records.
    QVERIFY(store.loadCatalogCache().has_value());
    QFile stagedSidecar(stagedSidecarPath);
    QVERIFY(stagedSidecar.open(QIODevice::ReadOnly));
    QCOMPARE(stagedSidecar.readAll(), QByteArray("staged raw payload"));
}

void SkyCatalogCacheMigrationTests::logsCollectionLifecycleSummariesAtInfoLevel()
{
    SkySettingsStore store;
    const SkyCatalogCacheController controller(&store);
    skygate::testsupport::LogCapture capture(QtInfoMsg);

    QVERIFY(store.saveCatalogCollectionCache(CatalogCacheFixture::makeCollectionSnapshot()));
    const auto result = controller.restoreCollection(0, 0, QString(), QString());
    QVERIFY(result.restored);
    QVERIFY(store.clearCatalogCollectionCache());

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache saved: sources 2")));
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache loaded: sources 2")));
    QVERIFY(messages.contains(QStringLiteral("Catalog collection cache cleared: files")));
}

QTEST_GUILESS_MAIN(SkyCatalogCacheMigrationTests)

#include "SkyCatalogCacheMigrationTests.moc"
