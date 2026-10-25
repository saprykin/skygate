#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogCacheTestSupport.hpp"
#include "support/CatalogCollectionCacheFixture.hpp"
#include <skygate/testsupport/LogCapture.hpp>
#include "support/SettingsTestFixture.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <utility>

using skygate::ui::tests::CatalogCollectionCacheFixture;

namespace {

SkyCatalogSourceCacheRecord makeCollectionRecord(
    const QString& instanceId, const QString& title, const QByteArray& rawPayload, const QByteArray& binaryPayload
)
{
    SkyCatalogSourceCacheRecord record;
    record.instanceId = instanceId;
    record.title = title;
    record.urls = QStringList{QStringLiteral("https://example.test/%1.csv").arg(instanceId)};
    record.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    record.enabled = true;
    record.payload = rawPayload;
    record.binaryPayload = binaryPayload;
    return record;
}

SkyCatalogCollectionCacheSnapshot makeCollectionSnapshot(QVector<SkyCatalogSourceCacheRecord> records)
{
    SkyCatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    for (std::size_t index = 0; index < records.size(); ++index) {
        records[index].order = static_cast<int>(index);
    }
    snapshot.sources = std::move(records);
    return snapshot;
}

// Two committed sources whose titles, raw bytes, and binary bytes all differ
// from the update a failure test attempts, so a mixed snapshot is visible.
SkyCatalogCollectionCacheSnapshot oldCollectionSnapshot()
{
    return makeCollectionSnapshot(
        {makeCollectionRecord(
             QStringLiteral("custom:alpha"),
             QStringLiteral("Old alpha"),
             QByteArray("old raw alpha"),
             QByteArray("old binary alpha")
         ),
         makeCollectionRecord(
             QStringLiteral("custom:beta"),
             QStringLiteral("Old beta"),
             QByteArray("old raw beta"),
             QByteArray("old binary beta")
         )}
    );
}

SkyCatalogCollectionCacheSnapshot newCollectionSnapshot()
{
    return makeCollectionSnapshot(
        {makeCollectionRecord(
             QStringLiteral("custom:alpha"),
             QStringLiteral("New alpha"),
             QByteArray("new raw alpha"),
             QByteArray("new binary alpha")
         ),
         makeCollectionRecord(
             QStringLiteral("custom:beta"),
             QStringLiteral("New beta"),
             QByteArray("new raw beta"),
             QByteArray("new binary beta")
         )}
    );
}

void compareCollectionRecords(
    const SkyCatalogCollectionCacheSnapshot& loaded, const SkyCatalogCollectionCacheSnapshot& expected
)
{
    QCOMPARE(loaded.sources.size(), expected.sources.size());
    for (std::size_t index = 0; index < expected.sources.size(); ++index) {
        QCOMPARE(loaded.sources[index].instanceId, expected.sources[index].instanceId);
        QCOMPARE(loaded.sources[index].title, expected.sources[index].title);
        QCOMPARE(loaded.sources[index].payload, expected.sources[index].payload);
        QCOMPARE(loaded.sources[index].binaryPayload, expected.sources[index].binaryPayload);
    }
}

}  // namespace

class SkyCollectionGenerationTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void freshStoreReportsNoCommittedCollection();
    void emptyCatalogCollectionCacheIsExplicitlyPersisted();
    void failedCollectionSaveKeepsLegacyCacheUnmigrated();
    void legacyFlatCollectionRecordsStillLoad();
    void emptyLegacyCollectionStillLoadsWithoutArtifacts();
    void missingManifestWithRecordsSkipsEmptyCollection();
    void missingManifestWithSidecarsLogsManifestLoss();
    void lostManifestAfterCommittedCollectionNotFirstUse();
    void stageOnlyGenerationIsNotProofOfCommit();
    void truncatedManifestIsUnusableButRecoverable();
    void missingCommittedGenerationRecordsAreUnusable();
    void invalidPersistedPolicyFallsBackToMerge();
    void invalidRecordsStoredMarkerIsUnusable();
    void failedLaterSourceRawWriteKeepsCommittedSnapshot();
    void failedSourceBinaryWriteKeepsCommittedSnapshot();
    void failedMetadataPublicationKeepsCommittedSnapshot();
    void failedManifestPublicationKeepsGenerationIntact();
    void successfulUpdateReplacesCommittedGeneration();
    void failedCollectionSaveLogsExplicitError();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCollectionGenerationTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCollectionGenerationTests")));
}

void SkyCollectionGenerationTests::freshStoreReportsNoCommittedCollection()
{
    CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    const SkySettingsStore store;
    const auto firstUse = store.loadCatalogCollectionCache();
    QCOMPARE(firstUse.state, SkyCatalogCollectionCacheLoadResult::State::Absent);
    QVERIFY(firstUse.diagnostic.isEmpty());
    QVERIFY(firstUse.snapshot.sources.isEmpty());
}

void SkyCollectionGenerationTests::emptyCatalogCollectionCacheIsExplicitlyPersisted()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();

    SkySettingsStore store;
    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(skygate::ui::tests::sampleCatalogCacheSnapshot()));

    SkyCatalogCollectionCacheSnapshot emptySnapshot;
    emptySnapshot.schemaVersion = skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;
    QVERIFY(store.saveCatalogCollectionCache(emptySnapshot));

    // An intentionally empty collection is stored, not erased: the committed
    // manifest distinguishes it from "no collection was ever stored" and keeps
    // the legacy two-slot cache retired.
    const auto loaded = store.loadCatalogCollectionCache();
    QCOMPARE(loaded.state, SkyCatalogCollectionCacheLoadResult::State::Loaded);
    QVERIFY(loaded.snapshot.sources.isEmpty());
    QCOMPARE(loaded.snapshot.schemaVersion, skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion);

    // A second restart persists the same intentionally empty configuration.
    QVERIFY(store.saveCatalogCollectionCache(emptySnapshot));
    const auto reloaded = store.loadCatalogCollectionCache();
    QCOMPARE(reloaded.state, SkyCatalogCollectionCacheLoadResult::State::Loaded);
    QVERIFY(reloaded.snapshot.sources.isEmpty());

    // The legacy cache stays readable; the committed collection retires the
    // fallback without deleting recoverable data.
    QVERIFY(store.loadCatalogCache().has_value());

    // Resetting the stored configuration is the operation that removes the
    // committed collection again.
    QVERIFY(store.clearCatalogCollectionCache());
    QCOMPARE(store.loadCatalogCollectionCache().state, SkyCatalogCollectionCacheLoadResult::State::Absent);
}

void SkyCollectionGenerationTests::failedCollectionSaveKeepsLegacyCacheUnmigrated()
{
    m_settings.resetSettingsWithCatalogCachePaths();

    SkySettingsStore store;
    const auto legacySnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Legacy custom"), .deepSkySourceLabel = QStringLiteral("Legacy OpenNGC")}
    );
    QVERIFY(skygate::ui::tests::seedLegacyCatalogCache(legacySnapshot));

    // Point the collection cache directory under a regular file so the payload
    // sidecar write fails before any collection metadata is committed.
    const QString blockerPath = m_settings.filePath(QStringLiteral("collection-blocker"));
    QFile blocker(blockerPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    blocker.write("x");
    blocker.close();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), blockerPath + QStringLiteral("/nested/cache")
    );

    QVERIFY(!store.saveCatalogCollectionCache(CatalogCollectionCacheFixture::sampleCollectionSnapshot()));

    // The failed save neither records a completed migration nor destroys the
    // last readable legacy data.
    const auto failedLoad = store.loadCatalogCollectionCache();
    QCOMPARE(failedLoad.state, SkyCatalogCollectionCacheLoadResult::State::Absent);
    const auto stillReadable = store.loadCatalogCache();
    QVERIFY(stillReadable.has_value());
    QCOMPARE(stillReadable->sourceLabel, legacySnapshot.sourceLabel);
    QCOMPARE(stillReadable->catalogPayload, legacySnapshot.catalogPayload);
    QCOMPARE(stillReadable->deepSkyCatalogPayload, legacySnapshot.deepSkyCatalogPayload);
}

void SkyCollectionGenerationTests::legacyFlatCollectionRecordsStillLoad()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);
    QVERIFY(QDir().mkpath(directory));

    const QString payloadPath = QDir(directory).filePath(QStringLiteral("legacy-payload.txt"));
    QFile payloadFile(payloadPath);
    QVERIFY(payloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(payloadFile.write("legacy raw payload"), qint64(18));
    payloadFile.close();

    // A collection written by an earlier release keeps its version marker and
    // record groups directly in the settings file, without a generation
    // manifest. Such settings must still load after this change.
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
    settings.setValue(QStringLiteral("skyContext/catalogBinarySchemaVersion"), 7);
    settings.beginGroup(QStringLiteral("catalogSources/catalog-source-legacy"));
    settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:legacy"));
    settings.setValue(QStringLiteral("title"), QStringLiteral("Legacy title"));
    settings.setValue(QStringLiteral("payloadPath"), payloadPath);
    settings.endGroup();
    settings.sync();

    skygate::testsupport::LogCapture capture;
    SkySettingsStore store;
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QCOMPARE(loaded.snapshot.schemaVersion, 2);
    QCOMPARE(loaded.snapshot.binarySchemaVersion, 7);
    QCOMPARE(loaded.snapshot.sources.size(), 1);
    QCOMPARE(loaded.snapshot.sources[0].instanceId, QString("custom:legacy"));
    QCOMPARE(loaded.snapshot.sources[0].title, QString("Legacy title"));
    QCOMPARE(loaded.snapshot.sources[0].payload, QByteArray("legacy raw payload"));

    // This settings file has no generation-format artifacts, so the absent
    // manifest is the normal pre-generation state and must stay quiet.
    const QString messages = capture.joinedMessages();
    QVERIFY(!messages.contains(QStringLiteral("Catalog collection manifest is missing")));
    QVERIFY(!messages.contains(QStringLiteral("stale catalog collection version marker")));
}

void SkyCollectionGenerationTests::emptyLegacyCollectionStillLoadsWithoutArtifacts()
{
    CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    // A pre-migration settings file can also record a committed-but-empty
    // collection: the marker is present while no flat record groups and no
    // generation artifacts exist. That empty snapshot stays the answer and the
    // absent manifest is not a loss to report.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 3);
        settings.sync();
    }

    skygate::testsupport::LogCapture capture;
    const SkySettingsStore store;
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QVERIFY(loaded.snapshot.sources.isEmpty());
    QCOMPARE(loaded.snapshot.schemaVersion, 3);
    QVERIFY(!capture.joinedMessages().contains(QStringLiteral("Catalog collection manifest is missing")));
}

void SkyCollectionGenerationTests::missingManifestWithRecordsSkipsEmptyCollection()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    // A settings file written by an earlier release carried the legacy
    // collection marker and its flat records. The generation-format save
    // migrates the records but leaves the marker behind, so the marker alone
    // no longer describes what is stored.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("skyContext/catalogCollectionVersion"), 2);
        settings.beginGroup(QStringLiteral("catalogSources/catalog-source-legacy"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:legacy"));
        settings.setValue(QStringLiteral("title"), QStringLiteral("Legacy title"));
        settings.endGroup();
        settings.sync();
    }

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const QString rawSidecarPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QFile rawSidecar(rawSidecarPath);
    QVERIFY(rawSidecar.open(QIODevice::ReadOnly));
    const QByteArray committedRawPayload = rawSidecar.readAll();
    rawSidecar.close();

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QByteArray committedManifest = manifestFile.readAll();
    manifestFile.close();

    const QDir cacheDir(directory);
    const QStringList sidecarsBefore = cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files);
    QCOMPARE(sidecarsBefore.size(), 4);
    QVERIFY(QFile::remove(manifestPath));

    // Without the manifest the stale marker points at flat records the
    // migration already replaced. Settings written before commit knowledge
    // existed carry no record of the committed generation, so only the stale
    // marker remains; reporting an empty collection instead of an unusable
    // committed state would let its caller commit that emptiness over the
    // committed generation.
    {
        QSettings settings;
        settings.remove(QStringLiteral("skyContext/catalogCollectionCommittedGeneration"));
        settings.sync();
    }
    skygate::testsupport::LogCapture capture;
    const auto unreadable = store.loadCatalogCollectionCache();
    QCOMPARE(unreadable.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    QVERIFY(!unreadable.diagnostic.isEmpty());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog collection manifest is missing")));
    QVERIFY(messages.contains(QStringLiteral("stale catalog collection version marker")));

    // Loading nothing is not an implicit cleanup: every sidecar is still
    // present with its bytes and the committed records were not touched.
    QCOMPARE(cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*")}, QDir::Files), sidecarsBefore);
    QFile rawSidecarAfter(rawSidecarPath);
    QVERIFY(rawSidecarAfter.open(QIODevice::ReadOnly));
    QCOMPARE(rawSidecarAfter.readAll(), committedRawPayload);
    rawSidecarAfter.close();
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("catalogSources/%1").arg(committedGeneration));
        QCOMPARE(settings.childGroups().size(), 2);
        settings.endGroup();
    }

    // Restoring the manifest alone brings the committed collection back,
    // proving the unusable result above left nothing else behind.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifest), qint64(committedManifest.size()));
    restoredManifest.close();
    const auto reloaded = store.loadCatalogCollectionCache();
    QVERIFY(reloaded.isLoaded());
    compareCollectionRecords(reloaded.snapshot, committed);
}

void SkyCollectionGenerationTests::missingManifestWithSidecarsLogsManifestLoss()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));

    // A settings reset can remove the generation records and the commit record
    // while the sidecar files survive. The leftover files are not proof that a
    // collection was committed, so the outcome stays first use, but the data
    // that was ignored must not be silent.
    {
        QSettings settings;
        settings.remove(QStringLiteral("catalogSources"));
        settings.remove(QStringLiteral("skyContext/catalogCollectionCommittedGeneration"));
        settings.sync();
    }
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(directory)));

    skygate::testsupport::LogCapture capture;
    const auto stagedLeftovers = store.loadCatalogCollectionCache();
    QCOMPARE(stagedLeftovers.state, SkyCatalogCollectionCacheLoadResult::State::Absent);
    QVERIFY(!stagedLeftovers.diagnostic.isEmpty());
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Catalog collection manifest is missing")));
}

void SkyCollectionGenerationTests::lostManifestAfterCommittedCollectionNotFirstUse()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // The commit record survives on its own and names the published generation.
    {
        QSettings settings;
        QCOMPARE(
            settings.value(QStringLiteral("skyContext/catalogCollectionCommittedGeneration")).toULongLong(),
            committedGeneration
        );
    }

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();
    QVERIFY(QFile::remove(manifestPath));

    const auto lostManifest = store.loadCatalogCollectionCache();
    QCOMPARE(lostManifest.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    QVERIFY(lostManifest.diagnostic.contains(QStringLiteral("recorded as committed")));

    // The recorded generation is a proven recovery point: restoring the
    // manifest brings the committed collection back unchanged.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    compareCollectionRecords(recovered.snapshot, committed);
}

void SkyCollectionGenerationTests::stageOnlyGenerationIsNotProofOfCommit()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);
    QVERIFY(QDir().mkpath(directory));

    // A first save interrupted before its manifest was published leaves staged
    // records and a sidecar behind. Nothing proves that a collection was ever
    // committed, so the outcome stays first use.
    {
        QSettings settings;
        settings.setValue(QStringLiteral("catalogSources/4/recordsStored"), true);
        settings.beginGroup(QStringLiteral("catalogSources/4/catalog-source-staged"));
        settings.setValue(QStringLiteral("instanceId"), QStringLiteral("custom:staged"));
        settings.endGroup();
        settings.sync();
    }
    QFile stagedSidecar(
        skygate::ui::tests::stagedCatalogSourceSidecarPath(
            directory, 4, QStringLiteral("custom:staged"), QStringLiteral(".txt")
        )
    );
    QVERIFY(stagedSidecar.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(stagedSidecar.write("staged raw payload"), qint64(18));
    stagedSidecar.close();

    SkySettingsStore store;
    const auto firstUse = store.loadCatalogCollectionCache();
    QCOMPARE(firstUse.state, SkyCatalogCollectionCacheLoadResult::State::Absent);
    QVERIFY(!firstUse.diagnostic.isEmpty());

    // Once a manifest has committed a generation, losing it is unusable
    // committed configuration instead of first use.
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));
    QVERIFY(QFile::remove(skygate::ui::tests::catalogCollectionManifestFilePath(directory)));
    const auto lostCommitted = store.loadCatalogCollectionCache();
    QCOMPARE(lostCommitted.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
}

void SkyCollectionGenerationTests::truncatedManifestIsUnusableButRecoverable()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();

    // A truncated manifest is committed configuration that cannot be read, not
    // first use, even while the committed generation itself stays intact.
    const QByteArray truncatedContent = QByteArrayLiteral("schemaVersion=1\nbinarySchemaVersion=4\ngenera");
    QFile truncatedManifest(manifestPath);
    QVERIFY(truncatedManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(truncatedManifest.write(truncatedContent), qint64(truncatedContent.size()));
    truncatedManifest.close();

    skygate::testsupport::LogCapture capture;
    const auto truncated = store.loadCatalogCollectionCache();
    QCOMPARE(truncated.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    QVERIFY(!truncated.diagnostic.isEmpty());
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Unreadable catalog collection manifest")));

    // Restoring the manifest recovers the committed collection unchanged.
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    compareCollectionRecords(recovered.snapshot, committed);
}

void SkyCollectionGenerationTests::missingCommittedGenerationRecordsAreUnusable()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // The manifest survives while the generation it names lost its records, as
    // after a partial settings reset; the payload files alone are not a
    // collection.
    {
        QSettings settings;
        settings.remove(QStringLiteral("catalogSources/%1").arg(committedGeneration));
        settings.sync();
    }

    skygate::testsupport::LogCapture capture;
    const auto missingRecords = store.loadCatalogCollectionCache();
    QCOMPARE(missingRecords.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    QVERIFY(missingRecords.diagnostic.contains(QString::number(committedGeneration)));
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("names a generation without stored records")));
}

void SkyCollectionGenerationTests::invalidPersistedPolicyFallsBackToMerge()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(CatalogCollectionCacheFixture::sampleCollectionSnapshot()));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    // Both records carry a policy the schema no longer names. The second one
    // was stored as a deep-sky policy, so a successful fallback is visible in
    // the loaded value rather than in a default that happens to match.
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("catalogSources/%1").arg(committedGeneration));
        const QStringList recordGroups = settings.childGroups();
        settings.endGroup();
        QCOMPARE(recordGroups.size(), 2);
        for (const QString& recordGroup : recordGroups) {
            settings.setValue(
                QStringLiteral("catalogSources/%1/%2/policy").arg(committedGeneration).arg(recordGroup), 200
            );
        }
        settings.sync();
    }

    skygate::testsupport::LogCapture capture;
    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    QCOMPARE(loaded.snapshot.sources.size(), 2);
    QCOMPARE(loaded.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(loaded.snapshot.sources[1].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Invalid catalog composition policy")));
}

void SkyCollectionGenerationTests::invalidRecordsStoredMarkerIsUnusable()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    {
        QSettings settings;
        settings.setValue(
            QStringLiteral("catalogSources/%1/recordsStored").arg(committedGeneration), QStringLiteral("maybe")
        );
        settings.sync();
    }

    // An unreadable marker is not proof that the generation's records were
    // stored, so the generation stays unusable and the warning names the
    // setting that failed to decode.
    skygate::testsupport::LogCapture capture;
    const auto loaded = store.loadCatalogCollectionCache();
    QCOMPARE(loaded.state, SkyCatalogCollectionCacheLoadResult::State::Unusable);
    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Invalid boolean setting")));
}

void SkyCollectionGenerationTests::failedLaterSourceRawWriteKeepsCommittedSnapshot()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    // Force the update's second source raw sidecar to fail after the first
    // source's new files were already staged.
    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".txt")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].payload != update.sources[0].payload);
}

void SkyCollectionGenerationTests::failedSourceBinaryWriteKeepsCommittedSnapshot()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".bin")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].binaryPayload != update.sources[0].binaryPayload);
}

void SkyCollectionGenerationTests::failedMetadataPublicationKeepsCommittedSnapshot()
{
    CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));

    // Payload staging still succeeds, but the settings write that publishes the
    // new records fails. The committed generation must stay active.
    const QString settingsDirectory = QFileInfo(QSettings().fileName()).absolutePath();
    QVERIFY(QFile::setPermissions(settingsDirectory, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto restorePermissions = qScopeGuard([settingsDirectory]() {
        QFile::setPermissions(
            settingsDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
        );
    });

    // Superuser rights bypass POSIX permission bits, so with them the
    // restrictive mode does not make the settings write fail and this
    // injection would test nothing. Probe the directory after the chmod and
    // skip instead of reporting a failure the injection cannot produce.
    const QString publicationProbePath = QDir(settingsDirectory).filePath(QStringLiteral("publication-probe"));
    QFile publicationProbe(publicationProbePath);
    if (publicationProbe.open(QIODevice::WriteOnly)) {
        publicationProbe.close();
        publicationProbe.remove();
        QSKIP(
            "Restrictive directory permissions do not block writes (running as root); the metadata publication "
            "failure cannot be injected"
        );
    }

    const auto update = newCollectionSnapshot();
    QVERIFY(!store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    QVERIFY(loaded.snapshot.sources[0].title != update.sources[0].title);
}

void SkyCollectionGenerationTests::failedManifestPublicationKeepsGenerationIntact()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const QString manifestPath = skygate::ui::tests::catalogCollectionManifestFilePath(directory);
    QFile committedManifest(manifestPath);
    QVERIFY(committedManifest.open(QIODevice::ReadOnly));
    const QByteArray committedManifestContent = committedManifest.readAll();
    committedManifest.close();
    QVERIFY(QFile::remove(manifestPath));
    QVERIFY(QDir().mkpath(manifestPath));

    QVERIFY(!store.saveCatalogCollectionCache(newCollectionSnapshot()));

    // The manifest is replaced atomically, so a failed publication leaves the
    // previous manifest in place. Restoring the recorded bytes shows that the
    // committed generation itself was not touched and its staged successor was
    // discarded.
    QVERIFY(QDir(manifestPath).removeRecursively());
    QFile restoredManifest(manifestPath);
    QVERIFY(restoredManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(restoredManifest.write(committedManifestContent), qint64(committedManifestContent.size()));
    restoredManifest.close();

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, committed);
    const QString stagedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration + 1U, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QVERIFY(!QFile::exists(stagedPath));
}

void SkyCollectionGenerationTests::successfulUpdateReplacesCommittedGeneration()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    const auto committed = oldCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(committed));
    const quint64 committedGeneration = skygate::ui::tests::committedCatalogCollectionGeneration(directory);
    QVERIFY(committedGeneration > 0U);

    const auto update = newCollectionSnapshot();
    QVERIFY(store.saveCatalogCollectionCache(update));

    const auto loaded = store.loadCatalogCollectionCache();
    QVERIFY(loaded.isLoaded());
    compareCollectionRecords(loaded.snapshot, update);

    // The committed snapshot is exactly the new generation: its payloads are
    // present and the previous generation's sidecars were removed.
    const QString currentRawPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration + 1U, QStringLiteral("custom:alpha"), QStringLiteral(".txt")
    );
    QVERIFY(QFile::exists(currentRawPath));
    const QString obsoleteBinaryPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory, committedGeneration, QStringLiteral("custom:beta"), QStringLiteral(".bin")
    );
    QVERIFY(!QFile::exists(obsoleteBinaryPath));
}

void SkyCollectionGenerationTests::failedCollectionSaveLogsExplicitError()
{
    const QString directory = CatalogCollectionCacheFixture::prepareCollectionCacheDirectory(m_settings);

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(oldCollectionSnapshot()));

    const QString blockedPath = skygate::ui::tests::stagedCatalogSourceSidecarPath(
        directory,
        skygate::ui::tests::committedCatalogCollectionGeneration(directory) + 1U,
        QStringLiteral("custom:beta"),
        QStringLiteral(".bin")
    );
    QVERIFY(QDir().mkpath(blockedPath));

    skygate::testsupport::LogCapture capture(QtInfoMsg);
    QVERIFY(!store.saveCatalogCollectionCache(newCollectionSnapshot()));

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Failed to stage catalog collection binary payload")));
    QVERIFY(messages.contains(QStringLiteral("committed collection remains active")));
    QVERIFY(!messages.contains(QStringLiteral("Catalog collection cache saved")));
}

QTEST_GUILESS_MAIN(SkyCollectionGenerationTests)

#include "SkyCollectionGenerationTests.moc"
