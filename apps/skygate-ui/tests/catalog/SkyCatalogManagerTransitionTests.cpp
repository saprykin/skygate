#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/SkyCatalogManager.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/CatalogBodyQuery.hpp"
#include "support/CatalogSourceFactsQuery.hpp"
#include "support/CatalogTestPayloadFile.hpp"
#include "support/CatalogTestPayloads.hpp"
#include "support/ConstellationAnchorGroupLookup.hpp"
#include "support/ConstellationTestSupport.hpp"
#include "support/FakeNetworkAccessManager.hpp"
#include "support/FakeReplyQuery.hpp"
#include "support/RelatedDatasetFixture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::CatalogTestPayloadFile;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

namespace {

struct NotificationEvent final {
    enum class Kind {
        StatusText,
        DatasetInfo,
        DeepSkyInfo,
        Catalog,
        Sources,
        Downloading,
    };

    Kind kind = Kind::StatusText;
    std::uint64_t revision = 0;
    std::size_t bodyCount = 0;
    std::size_t lineRefCount = 0;
    std::size_t constellationCount = 0;
    bool downloading = false;
};

void recordNotifications(SkyCatalogManager& manager, std::vector<NotificationEvent>& events)
{
    const auto capture = [&manager, &events](const NotificationEvent::Kind kind) {
        events.push_back(
            NotificationEvent{
                .kind = kind,
                .revision = manager.catalogRevision(),
                .bodyCount = manager.bodyCount(),
                .lineRefCount = manager.constellationLineRefs().size(),
                .constellationCount = manager.constellationCount(),
                .downloading = manager.downloadingCatalog(),
            }
        );
    };

    QObject::connect(&manager, &SkyCatalogManager::statusTextChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::StatusText);
    });
    QObject::connect(&manager, &SkyCatalogManager::datasetInfoTextChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::DatasetInfo);
    });
    QObject::connect(&manager, &SkyCatalogManager::deepSkyCatalogInfoTextChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::DeepSkyInfo);
    });
    QObject::connect(&manager, &SkyCatalogManager::catalogChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::Catalog);
    });
    QObject::connect(&manager, &SkyCatalogManager::sourcesChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::Sources);
    });
    QObject::connect(&manager, &SkyCatalogManager::downloadingCatalogChanged, &manager, [capture]() {
        capture(NotificationEvent::Kind::Downloading);
    });
}

const SkyCatalogManager::SourceViewEntry*
findSourceEntry(const QVector<SkyCatalogManager::SourceViewEntry>& entries, const QString& instanceId)
{
    const auto it = std::find_if(entries.begin(), entries.end(), [&instanceId](const auto& entry) {
        return entry.instanceId == instanceId;
    });
    return it != entries.end() ? &*it : nullptr;
}

std::size_t indexOfKind(
    const std::vector<NotificationEvent>& events, const NotificationEvent::Kind kind, const std::size_t from = 0
)
{
    for (std::size_t index = from; index < events.size(); ++index) {
        if (events[index].kind == kind) {
            return index;
        }
    }
    return events.size();
}

}  // namespace

class SkyCatalogManagerTransitionTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void reloadFromRowNotificationSupersedesStartup();
    void loadFromPublicationNotificationIsDropped();
    void failedLoadKeepsOtherPendingRelatedReply();
    void retryAfterFailureKeepsAttemptedDescriptors();
    void clearStarCacheEvictsOnlyMergeSources();
    void failedClearAttemptsRemainingSources();
    void completedLoadPublishesStatusThenRelatedData();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerTransitionTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerTransitionTests")));
}

void SkyCatalogManagerTransitionTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerTransitionTests::reloadFromRowNotificationSupersedesStartup()
{
    const QString startupUrl = QStringLiteral("https://example.test/reentrant-startup-stars.csv");
    const QString reloadUrl = QStringLiteral("https://example.test/reentrant-reload-stars.csv");
    const QByteArray reloadPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 911001, .hip = 911001, .properName = "Reentrant Reload Star", .mag = "1.0"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        startupUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 911000, .hip = 911000, .properName = "Reentrant Startup Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(reloadUrl, {.payload = reloadPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(startupUrl);
    source.title = QStringLiteral("Startup Title");
    const QString instanceId = source.instanceId;

    // The row notification is published before the transfer is issued, so a
    // receiver can synchronously replace the startup with a reload.
    bool reloadedFromRowNotification = false;
    QObject::connect(
        &manager,
        &SkyCatalogManager::sourcesChanged,
        &manager,
        [&manager, &source, &reloadUrl, &reloadedFromRowNotification]() {
            if (reloadedFromRowNotification) {
                return;
            }
            reloadedFromRowNotification = true;
            skygate::ui::internal::SkyCatalogSourceInstance reloaded = source;
            reloaded.title = QStringLiteral("Reload Title");
            reloaded.urls = QStringList{reloadUrl};
            manager.loadSource(reloaded, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        }
    );

    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(reloadedFromRowNotification);
    QVERIFY(manager.downloadingCatalog());
    // The superseded startup issued no request; only the nested reload did.
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList{reloadUrl});

    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(catalogSpy.count(), 1);
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Reentrant Reload Star")));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Reentrant Startup Star")));
    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Reload Title"));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const SkyCatalogManager::SourceViewEntry* entry = findSourceEntry(view, instanceId);
    QVERIFY(entry != nullptr);
    QVERIFY(!entry->busy);
    QVERIFY(!entry->hasError);
    QCOMPARE(entry->statusText, QStringLiteral("Active"));

    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Reload Title"));
    QCOMPARE(record->urls, QStringList{reloadUrl});
    QCOMPARE(record->payload, reloadPayload);
}

void SkyCatalogManagerTransitionTests::loadFromPublicationNotificationIsDropped()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/publication-accepted-stars.csv");
    const QString nestedUrl = QStringLiteral("https://example.test/publication-nested-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 911010, .hip = 911010, .properName = "Publication Accepted Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(
        nestedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 911011, .hip = 911011, .properName = "Publication Nested Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance nestedSource =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(nestedUrl);
    const QString nestedId = nestedSource.instanceId;

    // The publication notification is emitted before the completed transfer
    // stops being reported as active, so a load started from it is ignored.
    bool attemptedFromPublication = false;
    bool downloadingAtPublication = false;
    QObject::connect(
        &manager,
        &SkyCatalogManager::catalogChanged,
        &manager,
        [&manager, &nestedSource, &attemptedFromPublication, &downloadingAtPublication]() {
            if (attemptedFromPublication) {
                return;
            }
            attemptedFromPublication = true;
            downloadingAtPublication = manager.downloadingCatalog();
            manager.loadSource(nestedSource, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        }
    );

    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(attemptedFromPublication);
    QVERIFY(downloadingAtPublication);
    QCOMPARE(networkAccessManager.requestedUrls(), QStringList{acceptedUrl});
    QCOMPARE(catalogSpy.count(), 1);
    QVERIFY(!manager.sourceInstanceIds().contains(nestedId));
    QVERIFY(!CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Publication Nested Star")));

    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(CatalogSourceFactsQuery::findRecord(persisted.snapshot, nestedId) == nullptr);

    // Once the completed load has reset the download state, the same load is
    // accepted and the earlier attempt left no pending work behind.
    manager.loadSource(nestedSource, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(networkAccessManager.requestedUrls(), (QStringList{acceptedUrl, nestedUrl}));
    QCOMPARE(catalogSpy.count(), 2);
    QVERIFY(manager.sourceInstanceIds().contains(nestedId));
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Publication Nested Star")));
}

void SkyCatalogManagerTransitionTests::failedLoadKeepsOtherPendingRelatedReply()
{
    const QString ownerCatalogUrl = QStringLiteral("https://example.test/isolated-owner-stars.csv");
    const QString ownerRelatedUrl = QStringLiteral("https://example.test/isolated-owner-lines.json");
    const QString failingUrl = QStringLiteral("https://example.test/isolated-failing-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(ownerCatalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        ownerRelatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(
        failingUrl,
        {.error = QNetworkReply::ContentNotFoundError, .errorText = QStringLiteral("Not Found"), .httpStatusCode = 404}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance owner =
        RelatedDatasetFixture::relatedDatasetInstance(ownerCatalogUrl, ownerRelatedUrl);
    const QString ownerId = owner.instanceId;
    manager.loadSource(owner, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, ownerRelatedUrl);
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/isolated-failing-stars\\.csv .* HTTP 404")
    );
    const skygate::ui::internal::SkyCatalogSourceInstance failing =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(failingUrl);
    const QString failingId = failing.instanceId;
    manager.loadSource(failing, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The failed operation is terminal and reported, and it retired no other
    // instance's pending transfer.
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());
    QCOMPARE(catalogSpy.count(), 1);

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const SkyCatalogManager::SourceViewEntry* failedEntry = findSourceEntry(failedView, failingId);
    QVERIFY(failedEntry != nullptr);
    QVERIFY(failedEntry->hasError);
    QVERIFY(!failedEntry->busy);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("failed")));

    const SkyCatalogCollectionCacheLoadResult failedSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(failedSnapshot.isLoaded());
    QVERIFY(CatalogSourceFactsQuery::findRecord(failedSnapshot.snapshot, failingId) == nullptr);

    // The owner's reply completes afterwards and still becomes its published,
    // persisted related dataset.
    pendingReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QCOMPARE(catalogSpy.count(), 2);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const SkyCatalogManager::SourceViewEntry* ownerEntry = findSourceEntry(view, ownerId);
    QVERIFY(ownerEntry != nullptr);
    QVERIFY(!ownerEntry->busy);
    QVERIFY(!ownerEntry->hasError);
    QCOMPARE(ownerEntry->statusText, QStringLiteral("Active"));

    const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkyCatalogSourceCacheRecord* ownerRecord = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, ownerId);
    QVERIFY(ownerRecord != nullptr);
    QVERIFY(!ownerRecord->payload.isEmpty());
    QCOMPARE(CatalogSourceFactsQuery::relatedLineRefs(*ownerRecord).size(), std::size_t{2});
    QVERIFY(CatalogSourceFactsQuery::findRecord(snapshot.snapshot, failingId) == nullptr);
}

void SkyCatalogManagerTransitionTests::retryAfterFailureKeepsAttemptedDescriptors()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("attempted-descriptors.csv"));
    const QString relatedPath = m_settings.filePath(QStringLiteral("attempted-descriptors-lines.json"));
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();
    const QString relatedUrl = QUrl::fromLocalFile(relatedPath).toString();
    const QByteArray payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 911100, .hip = 911100, .properName = "Attempted Descriptor Star", .mag = "1.0"}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance attempt =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v9"));
    attempt.title = QStringLiteral("Attempted Title");
    attempt.descriptorId = QStringLiteral("attempted_descriptor");
    attempt.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    attempt.attribution = QStringLiteral("Attempted attribution");
    attempt.relatedDatasetUrls = QStringList{relatedUrl};
    const QString instanceId = attempt.instanceId;

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed file://.*/attempted-descriptors\\.csv .* HTTP 0")
    );
    manager.loadSource(attempt, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The rejected attempt is what the row reports, and it configured nothing
    // durable: the attempt is not a persisted source.
    QVERIFY(manager.sourceInstanceIds().count(instanceId) == 0);
    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const SkyCatalogManager::SourceViewEntry* failedEntry = findSourceEntry(failedView, instanceId);
    QVERIFY(failedEntry != nullptr);
    QVERIFY(failedEntry->hasError);
    QCOMPARE(failedEntry->title, QStringLiteral("Attempted Title"));
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());

    // A retry repeats the attempted descriptor, not the manager defaults.
    QVERIFY(CatalogTestPayloadFile::write(catalogPath, payload));
    QVERIFY(CatalogTestPayloadFile::write(relatedPath, RelatedDatasetFixture::orionRelatedDatasetPayload()));
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);

    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Attempted Descriptor Star")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Attempted Title"));

    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Attempted Title"));
    QCOMPARE(record->version, QStringLiteral("v9"));
    QCOMPARE(record->descriptorId, QStringLiteral("attempted_descriptor"));
    QCOMPARE(record->urls, QStringList{catalogUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(record->attribution, QStringLiteral("Attempted attribution"));
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QCOMPARE(record->payload, payload);
    QCOMPARE(CatalogSourceFactsQuery::relatedLineRefs(*record).size(), std::size_t{2});
}

void SkyCatalogManagerTransitionTests::clearStarCacheEvictsOnlyMergeSources()
{
    const QString starPath = m_settings.filePath(QStringLiteral("evict-scope-star.csv"));
    const QString deepSkyPath = m_settings.filePath(QStringLiteral("evict-scope-deep-sky.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            starPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 911200, .hip = 911200, .properName = "Evict Scope Star", .mag = "1.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            deepSkyPath,
            skygate::ui::tests::sampleOpenNgcCsvPayload(
                {.name = "NGC0702",
                 .type = "G",
                 .messier = "",
                 .ngc = "0702",
                 .identifiers = "PGC 7002",
                 .commonName = "Evict Scope Galaxy"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance star =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starPath).toString());
    star.title = QStringLiteral("Evict Star");
    const QString starId = star.instanceId;
    skygate::ui::internal::SkyCatalogSourceInstance deepSky =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(deepSkyPath).toString());
    deepSky.title = QStringLiteral("Evict Deep Sky");
    const QString deepSkyId = deepSky.instanceId;

    manager.loadSource(star, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(deepSky, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const SkyCatalogCollectionCacheLoadResult beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkyCatalogSourceCacheRecord* starBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, starId);
    const SkyCatalogSourceCacheRecord* deepSkyBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, deepSkyId);
    QVERIFY(starBeforeClear != nullptr);
    QVERIFY(deepSkyBeforeClear != nullptr);
    QVERIFY(!starBeforeClear->payload.isEmpty());
    QVERIFY(!deepSkyBeforeClear->payload.isEmpty());

    QVERIFY(manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Star catalog cache cleared"));

    // The accepted in-memory snapshots stay active for the session even though
    // the star source's disposable payload was evicted.
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Evict Scope Star")));
    QVERIFY(CatalogBodyQuery::containsId(manager.starCatalog(), QStringLiteral("ngc_702")));

    // Only the merge-policy source is evicted; the deep-sky source keeps its
    // payload, and both configured records stay durable.
    const SkyCatalogCollectionCacheLoadResult cleared = store.loadCatalogCollectionCache();
    QVERIFY(cleared.isLoaded());
    const SkyCatalogSourceCacheRecord* starCleared = CatalogSourceFactsQuery::findRecord(cleared.snapshot, starId);
    const SkyCatalogSourceCacheRecord* deepSkyCleared =
        CatalogSourceFactsQuery::findRecord(cleared.snapshot, deepSkyId);
    QVERIFY(starCleared != nullptr);
    QVERIFY(deepSkyCleared != nullptr);
    QVERIFY(starCleared->payload.isEmpty());
    QVERIFY(starCleared->binaryPayload.isEmpty());
    QCOMPARE(starCleared->title, QStringLiteral("Evict Star"));
    QCOMPARE(starCleared->urls, starBeforeClear->urls);
    QCOMPARE(starCleared->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!deepSkyCleared->payload.isEmpty());
    QVERIFY(!deepSkyCleared->binaryPayload.isEmpty());
    QCOMPARE(deepSkyCleared->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // A later metadata-only persistence keeps the eviction instead of writing
    // the retained in-memory payload back.
    manager.moveSource(deepSkyId, 1);
    const SkyCatalogCollectionCacheLoadResult reordered = store.loadCatalogCollectionCache();
    QVERIFY(reordered.isLoaded());
    const SkyCatalogSourceCacheRecord* starReordered = CatalogSourceFactsQuery::findRecord(reordered.snapshot, starId);
    QVERIFY(starReordered != nullptr);
    QVERIFY(starReordered->payload.isEmpty());
    QVERIFY(starReordered->binaryPayload.isEmpty());

    // Restart: the evicted source stays configured and reports the unavailable
    // payload and retry state, while the deep-sky source restores normally.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Saved catalog source cache has no payload")));
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds().count(starId), 1);
    QVERIFY(!CatalogBodyQuery::containsDisplayName(restarted.starCatalog(), QStringLiteral("Evict Scope Star")));
    QVERIFY(CatalogBodyQuery::containsId(restarted.starCatalog(), QStringLiteral("ngc_702")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = restarted.sourceViewEntries();
    const SkyCatalogManager::SourceViewEntry* evictedEntry = findSourceEntry(view, starId);
    const SkyCatalogManager::SourceViewEntry* restoredEntry = findSourceEntry(view, deepSkyId);
    QVERIFY(evictedEntry != nullptr);
    QVERIFY(restoredEntry != nullptr);
    QVERIFY(evictedEntry->hasError);
    QVERIFY(!evictedEntry->busy);
    QCOMPARE(evictedEntry->statusText, QStringLiteral("Payload unavailable"));
    QVERIFY(!restoredEntry->hasError);
}

void SkyCatalogManagerTransitionTests::failedClearAttemptsRemainingSources()
{
    const QString firstStarPath = m_settings.filePath(QStringLiteral("clear-failure-star-a.csv"));
    const QString secondStarPath = m_settings.filePath(QStringLiteral("clear-failure-star-b.csv"));
    const QString deepSkyPath = m_settings.filePath(QStringLiteral("clear-failure-deep-sky.csv"));
    QVERIFY(
        CatalogTestPayloadFile::write(
            firstStarPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 911400, .hip = 911400, .properName = "Clear Failure Star A", .mag = "1.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            secondStarPath,
            skygate::ui::tests::sampleHygCsvPayload(
                {.id = 911401, .hip = 911401, .properName = "Clear Failure Star B", .mag = "1.0"}
            )
        )
    );
    QVERIFY(
        CatalogTestPayloadFile::write(
            deepSkyPath,
            skygate::ui::tests::sampleOpenNgcCsvPayload(
                {.name = "NGC0703",
                 .type = "G",
                 .messier = "",
                 .ngc = "0703",
                 .identifiers = "PGC 7003",
                 .commonName = "Clear Failure Galaxy"}
            )
        )
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance firstStar =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(firstStarPath).toString());
    firstStar.title = QStringLiteral("Clear Failure A");
    const QString firstStarId = firstStar.instanceId;
    skygate::ui::internal::SkyCatalogSourceInstance secondStar =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(secondStarPath).toString());
    secondStar.title = QStringLiteral("Clear Failure B");
    const QString secondStarId = secondStar.instanceId;
    skygate::ui::internal::SkyCatalogSourceInstance deepSky =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(deepSkyPath).toString());
    deepSky.title = QStringLiteral("Clear Failure Deep Sky");
    const QString deepSkyId = deepSky.instanceId;

    manager.loadSource(firstStar, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(secondStar, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(deepSky, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QString cacheDirectory = m_settings.filePath(QStringLiteral("catalog-collection-cache"));
    const QString blockedSidecarPath =
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, firstStarId, QStringLiteral(".txt"));
    const QString clearedSidecarPath =
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, secondStarId, QStringLiteral(".txt"));
    const QString deepSkySidecarPath =
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, deepSkyId, QStringLiteral(".txt"));
    QVERIFY(!blockedSidecarPath.isEmpty());
    QVERIFY(!clearedSidecarPath.isEmpty());
    QVERIFY(!deepSkySidecarPath.isEmpty());

    const SkyCatalogCollectionCacheLoadResult beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkyCatalogSourceCacheRecord* firstBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, firstStarId);
    const SkyCatalogSourceCacheRecord* deepSkyBeforeClear =
        CatalogSourceFactsQuery::findRecord(beforeClear.snapshot, deepSkyId);
    QVERIFY(firstBeforeClear != nullptr);
    QVERIFY(deepSkyBeforeClear != nullptr);
    QVERIFY(!firstBeforeClear->payload.isEmpty());
    QVERIFY(!deepSkyBeforeClear->payload.isEmpty());

    // A non-empty directory where the source's payload sidecar is expected
    // cannot be removed as a file, so that source's eviction fails while the
    // other selected sources stay clearable.
    QVERIFY(QFile::remove(blockedSidecarPath));
    QVERIFY(QDir().mkpath(blockedSidecarPath));
    QFile blocker(QDir(blockedSidecarPath).filePath(QStringLiteral("keep")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QVERIFY(!manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Star catalog cache clear failed"));

    const SkyCatalogCollectionCacheLoadResult afterClear = store.loadCatalogCollectionCache();
    QVERIFY(afterClear.isLoaded());

    // The failed source keeps its configured record, and the merge-policy
    // source after it was still cleared.
    const SkyCatalogSourceCacheRecord* firstAfterClear =
        CatalogSourceFactsQuery::findRecord(afterClear.snapshot, firstStarId);
    const SkyCatalogSourceCacheRecord* secondAfterClear =
        CatalogSourceFactsQuery::findRecord(afterClear.snapshot, secondStarId);
    QVERIFY(firstAfterClear != nullptr);
    QVERIFY(secondAfterClear != nullptr);
    QCOMPARE(firstAfterClear->title, QStringLiteral("Clear Failure A"));
    QCOMPARE(secondAfterClear->title, QStringLiteral("Clear Failure B"));
    QVERIFY(secondAfterClear->payload.isEmpty());
    QVERIFY(secondAfterClear->binaryPayload.isEmpty());
    QVERIFY(!QFile::exists(clearedSidecarPath));

    // The deep-sky source is outside the star-clear policy and keeps both its
    // payload record and its sidecar file.
    const SkyCatalogSourceCacheRecord* deepSkyAfterClear =
        CatalogSourceFactsQuery::findRecord(afterClear.snapshot, deepSkyId);
    QVERIFY(deepSkyAfterClear != nullptr);
    QVERIFY(!deepSkyAfterClear->payload.isEmpty());
    QCOMPARE(deepSkyAfterClear->payload, deepSkyBeforeClear->payload);
    QVERIFY(QFile::exists(deepSkySidecarPath));
}

void SkyCatalogManagerTransitionTests::completedLoadPublishesStatusThenRelatedData()
{
    const QString catalogUrl = QStringLiteral("https://example.test/order-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/order-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<NotificationEvent> events;
    recordNotifications(manager, events);

    const skygate::ui::internal::SkyCatalogSourceInstance source =
        RelatedDatasetFixture::relatedDatasetInstance(catalogUrl, relatedUrl);
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);

    const std::size_t acceptedBodyCount = manager.bodyCount();
    const std::size_t catalogIndex = indexOfKind(events, NotificationEvent::Kind::Catalog);
    QVERIFY(catalogIndex < events.size());
    QVERIFY(catalogIndex >= 3);

    // Status text, then the accepted counts, then the accepted publication.
    QCOMPARE(events[catalogIndex - 3].kind, NotificationEvent::Kind::StatusText);
    QCOMPARE(events[catalogIndex - 2].kind, NotificationEvent::Kind::DatasetInfo);
    QCOMPARE(events[catalogIndex - 1].kind, NotificationEvent::Kind::DeepSkyInfo);
    QCOMPARE(events[catalogIndex - 2].bodyCount, acceptedBodyCount);
    QCOMPARE(events[catalogIndex - 3].revision, events[catalogIndex].revision);
    QCOMPARE(events[catalogIndex].bodyCount, acceptedBodyCount);
    QCOMPARE(events[catalogIndex].lineRefCount, std::size_t{0});
    QCOMPARE(events[catalogIndex].constellationCount, std::size_t{0});
    QVERIFY(events[catalogIndex].downloading);

    // The row notification follows the publication, which stays ordered before
    // the completed transfer stops being reported as active.
    const std::size_t rowIndex = catalogIndex + 1;
    QVERIFY(rowIndex < events.size());
    QCOMPARE(events[rowIndex].kind, NotificationEvent::Kind::Sources);
    const std::size_t downloadSettledIndex = rowIndex + 1;
    QVERIFY(downloadSettledIndex < events.size());
    QCOMPARE(events[downloadSettledIndex].kind, NotificationEvent::Kind::Downloading);
    QVERIFY(!events[downloadSettledIndex].downloading);
    QCOMPARE(events[downloadSettledIndex].lineRefCount, std::size_t{0});

    // The related dataset is published by a later notification group: counts,
    // then the related status, then the related publication.
    skygate::ui::tests::FakeNetworkReply* relatedReply =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(relatedReply != nullptr);
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const std::size_t relatedIndex = indexOfKind(events, NotificationEvent::Kind::Catalog, catalogIndex + 1);
    QVERIFY(relatedIndex < events.size());
    QVERIFY(relatedIndex > downloadSettledIndex);
    QVERIFY(relatedIndex >= catalogIndex + 3);
    QCOMPARE(events[relatedIndex - 2].kind, NotificationEvent::Kind::DatasetInfo);
    QCOMPARE(events[relatedIndex - 2].constellationCount, std::size_t{1});
    QCOMPARE(events[relatedIndex - 1].kind, NotificationEvent::Kind::StatusText);
    QCOMPARE(events[relatedIndex].bodyCount, acceptedBodyCount);
    QCOMPARE(events[relatedIndex].lineRefCount, std::size_t{2});
    QCOMPARE(events[relatedIndex].constellationCount, std::size_t{1});
    QVERIFY(!events[relatedIndex].downloading);
    QVERIFY(events[relatedIndex].revision > events[catalogIndex].revision);

    const auto catalogEventCount = std::count_if(events.begin(), events.end(), [](const NotificationEvent& event) {
        return event.kind == NotificationEvent::Kind::Catalog;
    });
    QCOMPARE(static_cast<std::size_t>(catalogEventCount), std::size_t{2});
}

QTEST_GUILESS_MAIN(SkyCatalogManagerTransitionTests)

#include "SkyCatalogManagerTransitionTests.moc"
