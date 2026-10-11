#include "CatalogCacheFixture.hpp"
#include "CatalogTestPayloads.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "catalog/SkyCatalogCacheController.hpp"

#include <QByteArray>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogSourceType;
using skygate::ui::internal::SkyCatalogCacheController;
using skygate::ui::internal::SkyCatalogCollectionPersistRequest;
using skygate::ui::internal::SkyCatalogCollectionRestoreResult;
using skygate::ui::internal::SkyCatalogSourcePersistEntry;
using skygate::ui::internal::SkyCatalogSourceRestoreEntry;
using skygate::ui::tests::CatalogCacheFixture;

namespace {

constexpr int kCurrentCollectionSchemaVersion =
    skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

const QString kArchiveSourceUrl = QStringLiteral("https://example.test/catalogs.zip");
const QString kStarsMember = QStringLiteral("catalog/hyg.csv");
const QString kDeepSkyMember = QStringLiteral("catalog/ngc.csv");

// A persisted source whose raw payload is a multi-member archive, with one
// member named by the descriptor's archive selection.
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

// The persisted snapshot nests its records under the committed generation
// when one was published, and keeps them directly under catalogSources for
// settings written by earlier releases.
QStringList persistedSourceRecordGroups(QSettings& settings)
{
    settings.beginGroup(QStringLiteral("catalogSources"));
    const QStringList topLevelGroups = settings.childGroups();
    settings.endGroup();

    QStringList recordGroups;
    for (const QString& group : topLevelGroups) {
        settings.beginGroup(QStringLiteral("catalogSources/") + group);
        const QStringList nestedGroups = settings.childGroups();
        settings.endGroup();
        if (nestedGroups.isEmpty()) {
            recordGroups.push_back(QStringLiteral("catalogSources/") + group);
            continue;
        }
        for (const QString& nestedGroup : nestedGroups) {
            recordGroups.push_back(QStringLiteral("catalogSources/") + group + QLatin1Char('/') + nestedGroup);
        }
    }
    return recordGroups;
}

// Removes the keys a record written before the parse contract was persisted
// cannot have, mirroring an older settings file.
QStringList removePersistedParseOptions()
{
    QSettings settings;
    const QStringList recordGroups = persistedSourceRecordGroups(settings);
    for (const QString& group : recordGroups) {
        settings.beginGroup(group);
        settings.remove(QStringLiteral("schemaHint"));
        settings.remove(QStringLiteral("attribution"));
        settings.endGroup();
    }
    settings.sync();
    return recordGroups;
}

}  // namespace

class SkyCatalogCacheRestoreTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresBinaryCatalogPayloadsWithoutUpgrade();
    void corruptBinaryPayloadFallsBackToCsvParsing();
    void legacyBinarySchemaVersionFallsBackToCsvParsing();
    void damagedSourceKeepsConfigurationAndSiblings();
    void restoresSelectedArchiveMemberWhenCacheMissing();
    void restoresSelectedArchiveMemberWhenCacheOutdated();
    void restoresSelectedArchiveMemberWhenCacheCorrupt();
    void optionFailuresKeepDamagedRecordsAsConfiguration();
    void unreadableSchemaHintFallsBackToDetection();
    void roundTripsParseOptionsDescriptorAndAttribution();
    void persistsAndRestoresBundledRecordsFromFactory();
    void olderRecordFormatRestoresDefinedDefaults();
    void roundTripsThreeEnabledSourcesPlusDisabledSource();
    void restoreDerivesDeepSkyFoundCountFromMixedSource();

private:
    void resetSettings();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogCacheRestoreTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogCacheRestoreTests")));
}

void SkyCatalogCacheRestoreTests::init()
{
    resetSettings();
}

void SkyCatalogCacheRestoreTests::resetSettings()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"), m_settings.filePath(QStringLiteral("collection-cache"))
    );
    QDir(m_settings.filePath(QStringLiteral("collection-cache"))).removeRecursively();
}

void SkyCatalogCacheRestoreTests::restoresBinaryCatalogPayloadsWithoutUpgrade()
{
    auto catalog = CatalogCacheFixture::makeStarCatalog("hip_11", "Alpha");

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

void SkyCatalogCacheRestoreTests::corruptBinaryPayloadFallsBackToCsvParsing()
{
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
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

void SkyCatalogCacheRestoreTests::legacyBinarySchemaVersionFallsBackToCsvParsing()
{
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
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

void SkyCatalogCacheRestoreTests::damagedSourceKeepsConfigurationAndSiblings()
{
    auto snapshot = CatalogCacheFixture::makeCollectionSnapshot();
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
    QVERIFY(stillPersisted.isLoaded());
    QCOMPARE(stillPersisted.snapshot.sources.size(), 2);
}

void SkyCatalogCacheRestoreTests::restoresSelectedArchiveMemberWhenCacheMissing()
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheRestoreTests::restoresSelectedArchiveMemberWhenCacheOutdated()
{
    // A binary sidecar written by an older binary schema is not trusted, so the
    // raw archive must be reparsed with the stored member selection. The
    // outdated sidecar deliberately holds the other archive member.
    auto snapshot = makeArchiveCollectionSnapshot();
    const auto otherMemberCatalog = CatalogCacheFixture::makeStarCatalog("hip_42", "Sirius");
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheRestoreTests::restoresSelectedArchiveMemberWhenCacheCorrupt()
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheRestoreTests::optionFailuresKeepDamagedRecordsAsConfiguration()
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[2].record.catalog), QStringLiteral("hip_42"));

    // The failing records stay persisted; a bad parse does not erase them.
    const auto stillPersisted = store.loadCatalogCollectionCache();
    QVERIFY(stillPersisted.isLoaded());
    QCOMPARE(stillPersisted.snapshot.sources.size(), 3);
}

void SkyCatalogCacheRestoreTests::unreadableSchemaHintFallsBackToDetection()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(makeArchiveCollectionSnapshot()));

    // A garbled hint must fall back to "no hint" so the payload is still read
    // by detection instead of failing with a synthetic mismatch.
    QSettings settings;
    const QStringList recordGroups = persistedSourceRecordGroups(settings);
    QCOMPARE(recordGroups.size(), 1);
    settings.beginGroup(recordGroups.first());
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*result.sources[0].record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheRestoreTests::roundTripsParseOptionsDescriptorAndAttribution()
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
    QVERIFY(stored.isLoaded());
    QCOMPARE(stored.snapshot.schemaVersion, kCurrentCollectionSchemaVersion);
    QCOMPARE(stored.snapshot.sources.size(), 1);
    QCOMPARE(stored.snapshot.sources[0].instanceId, QString("custom:archive"));
    QCOMPARE(stored.snapshot.sources[0].descriptorId, QString("archive_demo"));
    QCOMPARE(stored.snapshot.sources[0].version, QString("v2026.1"));
    QCOMPARE(stored.snapshot.sources[0].archiveSelector, kDeepSkyMember);
    QCOMPARE(stored.snapshot.sources[0].schemaHint, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(stored.snapshot.sources[0].attribution, QString("Demo archive attribution"));

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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*restored.record.catalog), QStringLiteral("messier_031"));
}

void SkyCatalogCacheRestoreTests::persistsAndRestoresBundledRecordsFromFactory()
{
    auto bundledCatalog = CatalogCacheFixture::makeStarCatalog("hip_42", "Sirius");
    auto downloadedCatalog = CatalogCacheFixture::makeStarCatalog("hip_43", "Vega");

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
    QVERIFY(stored.isLoaded());
    QCOMPARE(stored.snapshot.sources.size(), 2);
    QVERIFY(stored.snapshot.sources[0].bundled);
    QVERIFY(stored.snapshot.sources[0].payload.isEmpty());
    QVERIFY(stored.snapshot.sources[0].binaryPayload.isEmpty());
    QCOMPARE(stored.snapshot.sources[0].policy, CatalogCompositionPolicy::Merge);
    QVERIFY(!stored.snapshot.sources[0].enabled);
    QVERIFY(!stored.snapshot.sources[1].bundled);
    QVERIFY(!stored.snapshot.sources[1].payload.isEmpty());
    QVERIFY(!stored.snapshot.sources[1].binaryPayload.isEmpty());

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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*restoredDownloaded.record.catalog), QStringLiteral("hip_43"));

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

void SkyCatalogCacheRestoreTests::olderRecordFormatRestoresDefinedDefaults()
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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*restored.record.catalog), QStringLiteral("hip_42"));
    QCOMPARE(result.sources[1].record.instanceId, QString("custom:ambiguous"));
    QVERIFY(result.sources[1].record.catalog == nullptr);

    // Rewriting the upgraded records records the defaults once and ends the
    // migration boundary, mirroring the manager's persist after restore.
    controller.persistCollection(CatalogCacheFixture::persistRequestFromRestoreResult(result));
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.isLoaded());
    QCOMPARE(upgraded.snapshot.schemaVersion, kCurrentCollectionSchemaVersion);
    QCOMPARE(upgraded.snapshot.sources.size(), 2);
    QCOMPARE(upgraded.snapshot.sources[0].schemaHint, CatalogSourceType::Unknown);
    QVERIFY(upgraded.snapshot.sources[0].attribution.isEmpty());

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
    QCOMPARE(CatalogCacheFixture::firstBodyId(*secondResult.sources[0].record.catalog), QStringLiteral("hip_42"));
    QVERIFY(secondResult.sources[1].record.catalog == nullptr);
}

void SkyCatalogCacheRestoreTests::roundTripsThreeEnabledSourcesPlusDisabledSource()
{
    std::vector<std::unique_ptr<skygate::ephemeris::IStarCatalog>> catalogs;
    SkyCatalogCollectionPersistRequest request;
    const int hips[] = {1, 2, 3, 4};
    for (int index = 0; index < 4; ++index) {
        const std::string id = "hip_" + std::to_string(hips[index]);
        catalogs.push_back(CatalogCacheFixture::makeStarCatalog(id, "Star " + std::to_string(index)));

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

void SkyCatalogCacheRestoreTests::restoreDerivesDeepSkyFoundCountFromMixedSource()
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

QTEST_GUILESS_MAIN(SkyCatalogCacheRestoreTests)

#include "SkyCatalogCacheRestoreTests.moc"
