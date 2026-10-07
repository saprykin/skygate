#include "CatalogCacheTestSupport.hpp"
#include "CatalogDownloadWorkflowTestSupport.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QList>
#include <QLocale>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QThreadPool>
#include <QUrl>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool catalogContainsDisplayName(const skygate::ephemeris::IStarCatalog* catalog, const QString& displayName)
{
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&displayName](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->displayName) == displayName;
    });
}

bool catalogContainsId(const skygate::ephemeris::IStarCatalog* catalog, const QString& objectId)
{
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&objectId](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->id) == objectId;
    });
}

constexpr const char* kArchiveStarsMember = "catalog/stars.csv";
constexpr const char* kArchiveDeepSkyMember = "catalog/deep-sky.csv";

// A two-member archive whose members hold distinguishable objects, so a test
// can tell which member a restored source actually selected.
QByteArray archiveMemberZip(const skygate::ui::tests::DeepSkyCatalogPayloadOptions& deepSky)
{
    const QByteArray starsMember =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 900101, .properName = "Archive Member Star", .mag = "1.0"});
    const QByteArray deepSkyMember = skygate::ui::tests::sampleOpenNgcCsvPayload(deepSky);

    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{.path = kArchiveStarsMember, .data = starsMember.toStdString()},
        skygate::ephemeris::tests::ZipEntrySpec{.path = kArchiveDeepSkyMember, .data = deepSkyMember.toStdString()},
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

constexpr int kStaleConstellationDelayMs = 500;

// A source instance configured with one related constellation dataset, so a
// test can drive the owner-bound related-data lifecycle with fake replies.
skygate::ui::internal::SkyCatalogSourceInstance
relatedDatasetInstance(const QString& catalogUrl, const QString& relatedUrl)
{
    skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    instance.relatedDatasetUrls = QStringList{relatedUrl};
    return instance;
}

// One related constellation dataset with a single two-segment constellation,
// distinguishable per owner by its constellation id and anchor name.
QByteArray relatedDatasetPayload(const QString& constellationId, const QList<int>& hips)
{
    return skygate::ui::tests::stellariumConstellationIndexJsonPayload({{constellationId, hips}});
}

QByteArray orionRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("orion"), {27989, 25336, 25930});
}

QByteArray lyraRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("lyra"), {26311, 26727, 24436});
}

// -------------------------------------------------------------------------
// Interchangeability completion-gate fixture
// -------------------------------------------------------------------------
//
// Three configured sources exercised through the real import workflow:
//
// - two instances of the HYG descriptor. The first serves an anonymous payload
//   (no recognized designation, so every parse generates hyg_auto_1) and the
//   second serves two HIP rows with local record ids "1" and "2".
// - one archive source that explicitly selects one member of a two-member ZIP.
//   Its selected member overlaps HIP 70002 with the second instance under a
//   different local record id, so the shared authoritative identifier merges
//   while the local record ids of the two instances stay distinct.
//
// Each source owns its own related constellation dataset, so related data
// ownership is observable per instance.

constexpr const char* kScenarioAnonymousUrl = "https://example.test/scenario-anonymous.csv";
constexpr const char* kScenarioSecondUrl = "https://example.test/scenario-second.csv";
constexpr const char* kScenarioArchiveUrl = "https://example.test/scenario-archive.zip";
constexpr const char* kScenarioAnonymousRelatedUrl = "https://example.test/scenario-anonymous-lines.json";
constexpr const char* kScenarioSecondRelatedUrl = "https://example.test/scenario-second-lines.json";
constexpr const char* kScenarioArchiveRelatedUrl = "https://example.test/scenario-archive-lines.json";
constexpr const char* kScenarioArchiveMember = "catalog/hyg.csv";
constexpr const char* kScenarioArchiveUnselectedMember = "catalog/deep-sky.csv";
constexpr const char* kScenarioRemovableUrl = "https://example.test/scenario-removable.csv";

const skygate::ui::internal::SkyCatalogSourceDescriptor kScenarioDescriptor =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

struct InterchangeabilityScenarioSource final {
    skygate::ui::internal::SkyCatalogSourceInstance instance;
    QByteArray catalogPayload;
    QByteArray relatedPayload;
    QByteArray reloadedCatalogPayload;
    QByteArray reloadedRelatedPayload;
};

// The explicitly selected member and the unselected sibling member of the
// scenario archive. The sibling holds an object no scenario assertion expects,
// so selecting one member is observable.
QByteArray scenarioArchiveZip()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = kScenarioArchiveMember, .data = "id,hip,ra,dec,mag\n3,70002,6,6,5.5\n4,70003,7,7,6.0\n"
        },
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = kScenarioArchiveUnselectedMember,
            .data = skygate::ui::tests::sampleOpenNgcCsvPayload({.name = "NGC0999",
                                                                 .messier = "",
                                                                 .ngc = "0999",
                                                                 .identifiers = "PGC 9999",
                                                                 .commonName = "Unselected Member Galaxy"})
                        .toStdString()
        },
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

std::vector<InterchangeabilityScenarioSource> interchangeabilityScenarioSources()
{
    skygate::ui::internal::SkyCatalogSourceInstance anonymous =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(kScenarioDescriptor);
    anonymous.title = QStringLiteral("Anonymous Source");
    anonymous.urls = QStringList{QString::fromLatin1(kScenarioAnonymousUrl)};
    anonymous.relatedDatasetUrls = QStringList{QString::fromLatin1(kScenarioAnonymousRelatedUrl)};

    skygate::ui::internal::SkyCatalogSourceInstance second =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(kScenarioDescriptor);
    second.title = QStringLiteral("Second Instance");
    second.urls = QStringList{QString::fromLatin1(kScenarioSecondUrl)};
    second.relatedDatasetUrls = QStringList{QString::fromLatin1(kScenarioSecondRelatedUrl)};

    skygate::ui::internal::SkyCatalogSourceInstance archive =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(kScenarioDescriptor);
    archive.title = QStringLiteral("Archive Member");
    archive.version = QStringLiteral("v-archive");
    archive.urls = QStringList{QString::fromLatin1(kScenarioArchiveUrl)};
    archive.archiveSelector = QString::fromLatin1(kScenarioArchiveMember);
    archive.relatedDatasetUrls = QStringList{QString::fromLatin1(kScenarioArchiveRelatedUrl)};

    return {
        InterchangeabilityScenarioSource{
            .instance = std::move(anonymous),
            .catalogPayload = "ra,dec,mag\n1,2,3\n",
            .relatedPayload = relatedDatasetPayload(QStringLiteral("orion"), {70001, 70002}),
            .reloadedCatalogPayload = "ra,dec,mag\n3,4,5\n",
            .reloadedRelatedPayload = relatedDatasetPayload(QStringLiteral("corona"), {70001, 70002}),
        },
        InterchangeabilityScenarioSource{
            .instance = std::move(second),
            .catalogPayload = "id,hip,ra,dec,mag\n1,70001,5,5,4.0\n2,70002,6,6,5.0\n",
            .relatedPayload = relatedDatasetPayload(QStringLiteral("lyra"), {70002, 70001}),
        },
        InterchangeabilityScenarioSource{
            .instance = std::move(archive),
            .catalogPayload = scenarioArchiveZip(),
            .relatedPayload = relatedDatasetPayload(QStringLiteral("cygnus"), {70002, 70003}),
        },
    };
}

// A fourth source that exists only to be disabled, reloaded away, and removed
// without disturbing the identities of the three scenario sources.
InterchangeabilityScenarioSource interchangeabilityRemovableSource()
{
    skygate::ui::internal::SkyCatalogSourceInstance removable =
        skygate::ui::internal::SkyCatalogSourceInstance::fromDescriptor(kScenarioDescriptor);
    removable.title = QStringLiteral("Removable Source");
    removable.urls = QStringList{QString::fromLatin1(kScenarioRemovableUrl)};
    // The descriptor's preset related URLs are replaced by an empty selection
    // so the fixture never requests a live dataset.
    removable.relatedDatasetUrls.clear();

    return InterchangeabilityScenarioSource{
        .instance = std::move(removable),
        .catalogPayload = "id,hip,ra,dec,mag\n9,70005,9,9,7.0\n",
    };
}

void enqueueInterchangeabilityPayloads(
    skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager,
    const std::vector<InterchangeabilityScenarioSource>& sources
)
{
    for (const InterchangeabilityScenarioSource& source : sources) {
        networkAccessManager.enqueueResponse(source.instance.urls.first(), {.payload = source.catalogPayload});
        if (!source.relatedPayload.isEmpty()) {
            networkAccessManager.enqueueResponse(
                source.instance.relatedDatasetUrls.first(), {.payload = source.relatedPayload}
            );
        }
        if (!source.reloadedCatalogPayload.isEmpty()) {
            networkAccessManager.enqueueResponse(
                source.instance.urls.first(), {.payload = source.reloadedCatalogPayload}
            );
            networkAccessManager.enqueueResponse(
                source.instance.relatedDatasetUrls.first(), {.payload = source.reloadedRelatedPayload}
            );
        }
    }
}

void loadInterchangeabilityScenario(
    SkyCatalogManager& manager, const std::vector<InterchangeabilityScenarioSource>& sources
)
{
    std::size_t relatedSourceCount = 0;
    for (const InterchangeabilityScenarioSource& source : sources) {
        manager.loadSource(source.instance, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        QTRY_VERIFY(!manager.downloadingCatalog());
        if (!source.relatedPayload.isEmpty()) {
            ++relatedSourceCount;
        }
    }
    // The related datasets complete after their owning catalog.
    QTRY_COMPARE(manager.constellationAnchorGroups().size(), relatedSourceCount);
}

const skygate::ephemeris::BaseCelestialBody*
findBodyById(const skygate::ephemeris::IStarCatalog* catalog, const QString& id)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    const auto bodies = catalog->bodies();
    const auto it =
        std::find_if(bodies.begin(), bodies.end(), [&id](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && QString::fromStdString(body->id) == id;
        });
    return it == bodies.end() ? nullptr : *it;
}

std::optional<std::size_t> bodyIndexById(const skygate::ephemeris::IStarCatalog* catalog, const QString& id)
{
    if (catalog == nullptr) {
        return std::nullopt;
    }

    const auto bodies = catalog->bodies();
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && QString::fromStdString(bodies[index]->id) == id) {
            return index;
        }
    }
    return std::nullopt;
}

bool hasExternalIdentifier(
    const skygate::ephemeris::BaseCelestialBody& body, const QString& namespaceName, const QString& value
)
{
    return std::any_of(
        body.identity.externalIdentifiers.begin(),
        body.identity.externalIdentifiers.end(),
        [&namespaceName, &value](const skygate::ephemeris::CatalogIdentifier& identifier) {
            return QString::fromStdString(identifier.namespaceName) == namespaceName
                   && QString::fromStdString(identifier.value) == value;
        }
    );
}

bool hasAlias(const skygate::ephemeris::BaseCelestialBody& body, const QString& alias)
{
    return std::any_of(
        body.identity.aliases.begin(), body.identity.aliases.end(), [&alias](const std::string& existing) {
            return QString::fromStdString(existing) == alias;
        }
    );
}

// The committed records store the sidecar path each source was written with,
// so a test can address exactly one source's payloads without assuming how the
// committed generation names its files.
QString catalogSourceSidecarPath(const QString& directory, const QString& instanceId, const QString& extension)
{
    const QByteArray digest = QCryptographicHash::hash(instanceId.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString stem = QStringLiteral("catalog-source-") + QString::fromLatin1(digest.left(16));
    const QString pathKey =
        extension == QStringLiteral(".txt") ? QStringLiteral("payloadPath") : QStringLiteral("binaryPayloadPath");

    QSettings settings;
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

    const QString directoryPath = QFileInfo(directory).absoluteFilePath();
    for (const QString& recordGroup : recordGroups) {
        if (!recordGroup.endsWith(QStringLiteral("/") + stem)) {
            continue;
        }
        settings.beginGroup(recordGroup);
        const QString path = settings.value(pathKey).toString();
        settings.endGroup();
        if (!path.isEmpty() && QFileInfo(path).absolutePath() == directoryPath) {
            return path;
        }
    }
    return {};
}

// Restore marks a restored source title with a saved suffix; the durable title
// text behind that presentation suffix is what a restart must reproduce.
QString durableTitle(const QString& title)
{
    const QString savedSuffix = QStringLiteral(" (saved)");
    return title.endsWith(savedSuffix) ? title.left(title.size() - savedSuffix.size()) : title;
}

QStringList activeAnchorNames(const SkyCatalogManager& manager)
{
    QStringList names;
    for (const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup : manager.constellationAnchorGroups()) {
        names.push_back(QString::fromStdString(anchorGroup.first));
    }
    return names;
}

QStringList resolvedAnchorNames(const SkyCatalogManager& manager)
{
    QStringList names;
    for (const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup :
         manager.resolvedConstellationAnchorGroups()) {
        names.push_back(QString::fromStdString(anchorGroup.first));
    }
    return names;
}

QStringList resolvedLineRefTexts(const SkyCatalogManager& manager)
{
    QStringList lineRefs;
    for (const skygate::ephemeris::ConstellationLineRef& lineRef : manager.resolvedConstellationLineRefs()) {
        lineRefs.push_back(
            QString::fromStdString(lineRef.first) + QStringLiteral("|") + QString::fromStdString(lineRef.second)
        );
    }
    return lineRefs;
}

// Everything a restart must reproduce: collection order and participation,
// object identities with their winning source and contributors, and the
// resolved constellation view of the surviving bodies.
struct CollectionSnapshotFingerprint final {
    QStringList instanceIds;
    QStringList enabledFlags;
    QStringList titles;
    QStringList bodyIdentities;
    QStringList anchorNames;
    QStringList resolvedAnchorNames;
    QStringList resolvedLineRefs;
    std::size_t bodyCount = 0;
};

CollectionSnapshotFingerprint collectionFingerprint(const SkyCatalogManager& manager)
{
    CollectionSnapshotFingerprint fingerprint;
    fingerprint.instanceIds = manager.sourceInstanceIds();
    for (const QString& instanceId : fingerprint.instanceIds) {
        fingerprint.enabledFlags.push_back(
            instanceId + QStringLiteral("=")
            + (manager.isSourceEnabled(instanceId) ? QStringLiteral("enabled") : QStringLiteral("disabled"))
        );
    }
    const QHash<QString, QString> titles = manager.sourceTitles();
    for (const QString& instanceId : fingerprint.instanceIds) {
        fingerprint.titles.push_back(durableTitle(titles.value(instanceId)));
    }

    fingerprint.bodyCount = manager.bodyCount();
    if (const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog(); catalog != nullptr) {
        const auto bodies = catalog->bodies();
        const std::span<const QString> sourceIds = manager.sourceIds();
        const std::vector<QStringList>& contributorSourceIds = manager.contributorSourceIds();
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            const QString bodyId =
                bodies[index] != nullptr ? QString::fromStdString(bodies[index]->id) : QStringLiteral("<null>");
            const QString winningSourceId = index < sourceIds.size() ? sourceIds[index] : QString();
            const QString contributors =
                index < contributorSourceIds.size() ? contributorSourceIds[index].join(QStringLiteral(",")) : QString();
            fingerprint.bodyIdentities.push_back(
                bodyId + QStringLiteral("|") + winningSourceId + QStringLiteral("|") + contributors
            );
        }
    }

    fingerprint.anchorNames = activeAnchorNames(manager);
    fingerprint.resolvedAnchorNames = resolvedAnchorNames(manager);
    fingerprint.resolvedLineRefs = resolvedLineRefTexts(manager);
    return fingerprint;
}

void compareCollectionFingerprints(
    const CollectionSnapshotFingerprint& expected, const CollectionSnapshotFingerprint& actual
)
{
    QCOMPARE(actual.instanceIds, expected.instanceIds);
    QCOMPARE(actual.enabledFlags, expected.enabledFlags);
    QCOMPARE(actual.titles, expected.titles);
    QCOMPARE(actual.bodyCount, expected.bodyCount);
    QCOMPARE(actual.bodyIdentities, expected.bodyIdentities);
    QCOMPARE(actual.anchorNames, expected.anchorNames);
    QCOMPARE(actual.resolvedAnchorNames, expected.resolvedAnchorNames);
    QCOMPARE(actual.resolvedLineRefs, expected.resolvedLineRefs);
}

bool collectionContainsInstanceId(
    const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId
)
{
    return std::any_of(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
}

const SkySettingsStore::CatalogSourceCacheRecord*
findCollectionRecord(const SkySettingsStore::CatalogCollectionCacheSnapshot& snapshot, const QString& instanceId)
{
    const auto it = std::find_if(
        snapshot.sources.begin(),
        snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    return it != snapshot.sources.end() ? &*it : nullptr;
}

void compareConfiguredSourceFields(
    const SkySettingsStore::CatalogSourceCacheRecord& actual, const SkySettingsStore::CatalogSourceCacheRecord& expected
)
{
    QCOMPARE(actual.instanceId, expected.instanceId);
    QCOMPARE(actual.descriptorId, expected.descriptorId);
    QCOMPARE(actual.title, expected.title);
    QCOMPARE(actual.version, expected.version);
    QCOMPARE(actual.url, expected.url);
    QCOMPARE(actual.urls, expected.urls);
    QCOMPARE(actual.relatedDatasetUrls, expected.relatedDatasetUrls);
    QCOMPARE(actual.archiveSelector, expected.archiveSelector);
    QCOMPARE(actual.schemaHint, expected.schemaHint);
    QCOMPARE(actual.attribution, expected.attribution);
    QCOMPARE(actual.policy, expected.policy);
    QCOMPARE(actual.enabled, expected.enabled);
    QCOMPARE(actual.bundled, expected.bundled);
    QCOMPARE(actual.order, expected.order);
}

std::vector<skygate::ephemeris::ConstellationLineRef>
relatedLineRefs(const SkySettingsStore::CatalogSourceCacheRecord& record)
{
    return skygate::ui::internal::SkyContextCatalogCodec::parseConstellationLineRows(
        std::string_view(
            record.constellationLineRows.constData(), static_cast<std::size_t>(record.constellationLineRows.size())
        )
    );
}

bool relatedLineRefsContainHip(
    const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs, const std::string& hipId
)
{
    return std::any_of(
        lineRefs.begin(), lineRefs.end(), [&hipId](const skygate::ephemeris::ConstellationLineRef& lineRef) {
            return lineRef.first == hipId || lineRef.second == hipId;
        }
    );
}

// Finds the active related anchor group by constellation name. The returned
// pointer stays valid until the next active-view mutation.
const skygate::ephemeris::ConstellationAnchorGroup*
findConstellationAnchorGroup(const SkyCatalogManager& manager, const std::string& name)
{
    for (const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup : manager.constellationAnchorGroups()) {
        if (anchorGroup.first == name) {
            return &anchorGroup;
        }
    }
    return nullptr;
}

const skygate::ui::internal::SkyCatalogSourceDescriptor kHygPreset =
    skygate::ui::internal::SkyCatalogPresets::starSourceDescriptor(QStringLiteral("hyg_v42")).value();

skygate::ui::tests::FakeNetworkReply*
findReplyForUrl(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager, const QString& url)
{
    for (skygate::ui::tests::FakeNetworkReply* reply : networkAccessManager.issuedReplies()) {
        if (reply != nullptr && reply->url().toString() == url) {
            return reply;
        }
    }
    return nullptr;
}

bool allIssuedRepliesFinished(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager)
{
    const auto replies = networkAccessManager.issuedReplies();
    return std::all_of(replies.begin(), replies.end(), [](const skygate::ui::tests::FakeNetworkReply* reply) {
        return reply == nullptr || reply->isFinished();
    });
}

// The reply still open for the URL. An earlier reply for the same URL may
// already have finished, so a replacement request needs the pending one.
skygate::ui::tests::FakeNetworkReply*
findPendingReplyForUrl(skygate::ui::tests::FakeNetworkAccessManager& networkAccessManager, const QString& url)
{
    for (skygate::ui::tests::FakeNetworkReply* reply : networkAccessManager.issuedReplies()) {
        if (reply != nullptr && !reply->isFinished() && reply->url().toString() == url) {
            return reply;
        }
    }
    return nullptr;
}

// What a consumer reads from the manager while rebuilding its view on a
// catalogChanged notification. Comparing a recorded read with the runtime
// state after the notifying operation returns proves that a notification never
// published a state that the runtime then silently moved away from.
struct CatalogNotificationState final {
    std::uint64_t revision = 0;
    std::size_t lineRefCount = 0;
    std::size_t anchorGroupCount = 0;
    std::size_t constellationCount = 0;
    std::size_t bodyCount = 0;
};

void observeCatalogNotifications(SkyCatalogManager& manager, std::vector<CatalogNotificationState>& observations)
{
    QObject::connect(&manager, &SkyCatalogManager::catalogChanged, &manager, [&manager, &observations] {
        observations.push_back(
            CatalogNotificationState{
                .revision = manager.catalogRevision(),
                .lineRefCount = manager.constellationLineRefs().size(),
                .anchorGroupCount = manager.constellationAnchorGroups().size(),
                .constellationCount = manager.constellationCount(),
                .bodyCount = manager.bodyCount(),
            }
        );
    });
}

void assertObservedStateMatchesRuntime(const SkyCatalogManager& manager, const CatalogNotificationState& observation)
{
    QCOMPARE(observation.revision, manager.catalogRevision());
    QCOMPARE(observation.lineRefCount, manager.constellationLineRefs().size());
    QCOMPARE(observation.anchorGroupCount, manager.constellationAnchorGroups().size());
    QCOMPARE(observation.constellationCount, manager.constellationCount());
    QCOMPARE(observation.bodyCount, manager.bodyCount());
}

}  // namespace

class SkyCatalogManagerTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void unknownPresetsUpdateStatus();
    void bundledPresetResetsCatalogAndConstellationRefs();
    void bundledReplacementDropsOwnerDataAndReportsFreshCounts();
    void bundledDeepSkyPresetRebuildsActiveCatalog();
    void customDeepSkyDownloadUsesGenericSourceLabel();
    void clearCacheReportsStatusAndSignals();
    void restoreCachePathThroughManager();
    void localCatalogDownloadTogglesBusyProcessingAndAppliesCatalog();
    void cancelCatalogDownloadClearsBusyAndIgnoresResult();
    void failedLocalCatalogDownloadClearsBusyAndReportsStatus();
    void staleConstellationResponseIgnoredAfterBundledSwitch();
    void staleConstellationResponseIgnoredAfterCustomSwitch();
    void cancelDuringConstellationLoadingIgnoresStaleCompletion();
    void currentConstellationResponseAppliesOnce();
    void collectionSourcesLoadEnableDisableAndRemoveIndependently();
    void failedCollectionLoadPreservesPriorDataAndRetrySucceeds();
    void addSourceUrlAddsSameCategorySourcesWithStableIdentity();
    void addSourcePresetUsesDescriptorPolicy();
    void moveSourceReordersActiveSources();
    void clearSourceCacheKeepsConfiguredRecords();
    void clearSourceCacheEvictsOwnedRelatedPayload();
    void relatedReplyAfterCacheEvictionStaysOutOfPersistedRecord();
    void sameDescriptorInstancesCoexistIndependently();
    void sameUrlDifferentVersionsStayDistinct();
    void editingSourceAsUpdatePreservesInstanceId();
    void failedUpdateKeepsAcceptedSourceFactsWhenPersisting();
    void acceptedSourceFactsSurviveRestartAfterFailedUpdate();
    void successfulRetryInstallsUpdateFactsTogether();
    void canceledUpdateKeepsAcceptedSourceFacts();
    void parseFailedUpdateKeepsAcceptedSourceFacts();
    void rejectedCompositionAfterParsedUpdateKeepsAcceptedSourceFacts();
    void failedUpdateSupersedesItsPendingRelatedResponse();
    void restoresLegacyPersistedInstanceIdsWithReferences();
    void legacyMigrationRenamesDuplicateInstanceIds();
    void restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss();
    void legacyBundledStarSlotSurvivesMigrationThroughManager();
    void persistsBundledSourceConfigurationWithoutNetworkOperation();
    void restoredBundledDeepSkySourceKeepsFreshObjectCount();
    void interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence();
    void unreadablePayloadKeepsConfiguredSourceWithoutErasingSiblings();
    void rejectedRestoreKeepsPreviousCollectionAndReportsError();
    void removedBundledSourceDoesNotReturnAfterRestart();
    void relatedConstellationDatasetsStayOwnedByTheirSources();
    void overlappingConstellationDatasetsFollowSourceOrder();
    void lateRelatedResponseAfterOwnerRemovalIsIgnored();
    void readdedOwnerRejectsPreviousIncarnationResponse();
    void disablingOwnerSupersedesItsPendingRelatedResponse();
    void lateRelatedFailureStatusFromSupersededOwnerIsIgnored();
    void outOfOrderRelatedRepliesPopulateTheirOwnSources();
    void reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse();
    void reloadClearsOwnerRelatedDataBeforePublishing();
    void failedRelatedReplacementKeepsPublishedStateSynchronized();
    void canceledRelatedReplacementKeepsPublishedStateSynchronized();
    void reloadClearsOnlyOwningRelatedDataset();
    void updateRemovingRelatedDeclarationRetiresOwnedData();
    void failedUpdateAttemptingToRemoveRelatedDeclarationKeepsDataset();
    void lateReplyToRemovedRelatedDeclarationIsRejected();
    void bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy_data();
    void bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy();
    void payloadOriginSwitchesFollowTheAcceptedRelatedDeclaration();
    void rejectedBundledUpdateKeepsOwnedRelatedData();
    void relatedDatasetsRoundTripToTheirOwnSources();
    void disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart();
    void removedOwnerRelatedDataDoesNotReturnAfterRestart();
    void corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact();
    void migratesPriorSingleOwnerRelatedPayloadOnce();
    void presentationSummarizesEnabledCollectionParticipation();
    void bundledFallbackPresentationFollowsParticipationAndRestart();
    void interchangeabilityScenarioKeepsObjectsProvenanceAndOwnedRelatedData();
    void sharedAliasNamesStayDistinctThroughManagerWorkflow();
    void conflictingAstrometryKeepsOneCoherentModelThroughManager();
    void bundledDeepSkyFallbackFillsGapsWithoutOverridingConfiguredChoice();
    void crossSourceHipBridgesSurviveBinaryCollectionRestore();
    void collectionLifecycleKeepsSnapshotAndIdentitiesAcrossRestart();
    void legacySourcesStayRetiredAfterMigratedCollectionIsEmptied();
    void unreadableCommittedCollectionKeepsActiveCollectionAndReportsFailure();
    void missingConfigurationKeepsFirstUseDefaults();
    void removingSoleSourceLeavesEmptyCollection();
    void savedEmptyCollectionRestoresEmptyAcrossRestarts();
    void combinedAcceptedFactsSurviveRelatedReplacementRejectionReorderAndRawRestart();
    void liveRestoreDropsOperationsOmittedFromTheRestoredCollection();
    void liveRestoreSupersedesPendingImportReply();
    void liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId();
    void rejectedRestoreKeepsPendingImportWork();

private:
    SkySettingsStore::CatalogCacheSnapshot makeCacheSnapshot() const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerTests")));
}

void SkyCatalogManagerTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

SkySettingsStore::CatalogCacheSnapshot SkyCatalogManagerTests::makeCacheSnapshot() const
{
    return skygate::ui::tests::sampleCatalogCacheSnapshot(
        {.sourceLabel = QStringLiteral("Custom"), .deepSkySourceLabel = QStringLiteral("OpenNGC")}
    );
}

void SkyCatalogManagerTests::unknownPresetsUpdateStatus()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset("unknown");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown preset 'unknown'"));
    QCOMPARE(statusSpy.count(), 1);

    manager.loadDeepSkyCatalogPreset("unknown_dso");
    QCOMPARE(manager.statusText(), QString("Catalog: Unknown deep-sky preset 'unknown_dso'"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::bundledPresetResetsCatalogAndConstellationRefs()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.setCatalogPresetIndex(99);
    manager.loadCatalogPreset("bundled");

    QCOMPARE(manager.catalogPresetIndex(), 0);
    QCOMPARE(manager.sourceLabel(), QString("Bundled"));
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(catalogSpy.count() >= 1);
}

void SkyCatalogManagerTests::bundledReplacementDropsOwnerDataAndReportsFreshCounts()
{
    const QString catalogUrl = QStringLiteral("https://example.test/bundled-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/bundled-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    // The legacy primary slot owns a related dataset before the bundled preset
    // replaces its catalog.
    skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    source.instanceId = QStringLiteral("primary");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationCount() > 0U);

    manager.loadCatalogPreset("bundled");

    // The bundled replacement drops the owned related dataset, and the status
    // text of that committed replacement reports the post-clear counts instead
    // of the pre-clear ones.
    QVERIFY(manager.constellationLineRefs().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().endsWith(
        QStringLiteral("%1 constellations)").arg(locale.toString(static_cast<qulonglong>(manager.constellationCount())))
    ));
}

void SkyCatalogManagerTests::bundledDeepSkyPresetRebuildsActiveCatalog()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const auto originalRevision = manager.catalogRevision();
    QSignalSpy infoSpy(&manager, &SkyCatalogManager::deepSkyCatalogInfoTextChanged);

    manager.setDeepSkyCatalogPresetIndex(99);
    manager.loadDeepSkyCatalogPreset("bundled_messier");

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 0);
    QVERIFY(manager.catalogRevision() > originalRevision);
    QVERIFY(manager.deepSkyCatalogInfoText().contains("Objects:"));
    QVERIFY(infoSpy.count() >= 1);
}

void SkyCatalogManagerTests::customDeepSkyDownloadUsesGenericSourceLabel()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-deep-sky.csv"));
    QVERIFY(writeFile(catalogPath, skygate::ui::tests::sampleOpenNgcCsvPayload()));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    manager.downloadDeepSkyCatalogFromUrl(catalogUrl);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(manager.deepSkyCatalogUrlText(), catalogUrl);
    QVERIFY(manager.sourceTitles().values().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.sourceTitles().values().contains(QStringLiteral("OpenNGC")));
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(!manager.statusText().contains(QStringLiteral("OpenNGC")));
}

void SkyCatalogManagerTests::clearCacheReportsStatusAndSignals()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    QVERIFY(manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Star catalog cache cleared"));
    QVERIFY(manager.clearDeepSkyCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Deep-sky catalog cache cleared"));
    QCOMPARE(statusSpy.count(), 2);
}

void SkyCatalogManagerTests::restoreCachePathThroughManager()
{
    SkySettingsStore store;
    QVERIFY(store.saveCatalogCache(makeCacheSnapshot()));

    SkyCatalogManager manager(&store);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    const QString starUrl = QStringLiteral("https://example.test/custom-stars.csv");
    const QString deepSkyUrl = QStringLiteral("https://example.test/custom-dso.csv");
    manager.setCatalogPresetIndex(2);
    manager.setCatalogUrlText(starUrl);
    manager.setDeepSkyCatalogPresetIndex(2);
    manager.setDeepSkyCatalogUrlText(deepSkyUrl);

    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceLabel(), QString("Custom (saved)"));
    QCOMPARE(manager.constellationCount(), 1U);
    QCOMPARE(manager.constellationLineRefs().size(), 1U);
    QVERIFY(manager.bodyCount() > 0U);
    QCOMPARE(catalogSpy.count(), 1);
}

void SkyCatalogManagerTests::localCatalogDownloadTogglesBusyProcessingAndAppliesCatalog()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-local-stars.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload({.hip = 900001, .properName = "Manager Downloaded Star", .mag = "1.0"})
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy processingSpy(&manager, &SkyCatalogManager::catalogProcessingChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.downloadCatalogFromUrl(QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.downloadingCatalog());
    QVERIFY(!manager.clearCatalogCache());
    QCOMPARE(manager.statusText(), QString("Catalog: Cannot clear cache while download is in progress"));

    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(!manager.catalogProcessing());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QString("Manager Downloaded Star")));
    QCOMPARE(manager.catalogPresetIndex(), 2);
    QCOMPARE(manager.catalogUrlText(), QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.statusText().contains(QStringLiteral("Downloaded")));
    QVERIFY(downloadSpy.count() >= 2);
    QVERIFY(processingSpy.count() >= 2);
    QVERIFY(catalogSpy.count() >= 1);
    QVERIFY(statusSpy.count() >= 2);
}

void SkyCatalogManagerTests::cancelCatalogDownloadClearsBusyAndIgnoresResult()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("manager-cancel-stars.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload({
            .hip = 900002,
            .properName = "Canceled Manager Star",
            .mag = "1.0",
        })
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.downloadCatalogFromUrl(QUrl::fromLocalFile(catalogPath).toString());
    QVERIFY(manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> busyView = manager.sourceViewEntries();
    QVERIFY(!busyView.isEmpty());
    QVERIFY(busyView.first().busy);

    manager.cancelCatalogDownload();

    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(manager.statusText(), QString("Catalog: Download canceled."));

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = manager.sourceViewEntries();
    QVERIFY(!canceledView.isEmpty());
    QVERIFY(!canceledView.first().busy);
    QCOMPARE(canceledView.first().statusText, QStringLiteral("Canceled"));

    QCoreApplication::processEvents();
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QString("Canceled Manager Star")));
    QVERIFY(downloadSpy.count() >= 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerTests::failedLocalCatalogDownloadClearsBusyAndReportsStatus()
{
    const QString missingCatalogUrl =
        QUrl::fromLocalFile(m_settings.filePath(QStringLiteral("missing-stars.csv"))).toString();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const std::uint64_t originalRevision = manager.catalogRevision();
    QSignalSpy downloadSpy(&manager, &SkyCatalogManager::downloadingCatalogChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed file://.*/missing-stars\\.csv .* HTTP 0")
    );
    manager.downloadCatalogFromUrl(missingCatalogUrl);
    QVERIFY(manager.downloadingCatalog());

    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QVERIFY(manager.statusText().contains(QStringLiteral("failed"), Qt::CaseInsensitive));
    QVERIFY(manager.statusText().contains(missingCatalogUrl));
    QCOMPARE(manager.catalogRevision(), originalRevision);
    QCOMPARE(downloadSpy.count(), 2);
    QCOMPARE(catalogSpy.count(), 0);
}

void SkyCatalogManagerTests::staleConstellationResponseIgnoredAfterBundledSwitch()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900010, .properName = "Pending HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> constellationReply =
        findReplyForUrl(networkAccessManager, constellationUrl);
    QVERIFY(!constellationReply.isNull());
    QVERIFY(!constellationReply->isFinished());

    manager.loadCatalogPreset(QStringLiteral("bundled"));
    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    const auto revisionAfterSwitch = manager.catalogRevision();
    const int catalogChangesAfterSwitch = catalogSpy.count();
    const QString statusAfterSwitch = manager.statusText();
    const int statusChangesAfterSwitch = statusSpy.count();

    if (!constellationReply.isNull()) {
        skygate::ui::tests::waitForFakeReplyFinished(constellationReply.data());
    }
    QCoreApplication::processEvents();

    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterSwitch);
    QCOMPARE(manager.statusText(), statusAfterSwitch);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterSwitch);
    QCOMPARE(statusSpy.count(), statusChangesAfterSwitch);
}

void SkyCatalogManagerTests::staleConstellationResponseIgnoredAfterCustomSwitch()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    const QString customUrl = QStringLiteral("https://example.test/custom-stars.csv");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900011, .properName = "Pending HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );
    networkAccessManager.enqueueResponse(
        customUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 900012, .properName = "Custom Star", .mag = "2.0"})}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> constellationReply =
        findReplyForUrl(networkAccessManager, constellationUrl);
    QVERIFY(!constellationReply.isNull());
    QVERIFY(!constellationReply->isFinished());

    manager.downloadCatalogFromUrl(customUrl);
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("Downloaded"));
    QVERIFY(manager.constellationLineRefs().empty());
    const auto revisionAfterSwitch = manager.catalogRevision();
    const int catalogChangesAfterSwitch = catalogSpy.count();
    const QString statusAfterSwitch = manager.statusText();
    const int statusChangesAfterSwitch = statusSpy.count();

    if (!constellationReply.isNull()) {
        skygate::ui::tests::waitForFakeReplyFinished(constellationReply.data());
    }
    QCoreApplication::processEvents();

    QCOMPARE(manager.sourceLabel(), QStringLiteral("Downloaded"));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterSwitch);
    QCOMPARE(manager.statusText(), statusAfterSwitch);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterSwitch);
    QCOMPARE(statusSpy.count(), statusChangesAfterSwitch);

    const auto cacheSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(cacheSnapshot.isLoaded());
    QCOMPARE(cacheSnapshot.snapshot.sources.size(), 1);
    QVERIFY(cacheSnapshot.snapshot.sources[0].constellationLineRows.isEmpty());
    QVERIFY(cacheSnapshot.snapshot.sources[0].constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerTests::cancelDuringConstellationLoadingIgnoresStaleCompletion()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.hip = 900013, .properName = "Cancel Pending HYG Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl,
        {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload(), .delayMs = kStaleConstellationDelayMs}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(constellationUrl));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(!manager.downloadingCatalog());

    const auto revisionBeforeCancel = manager.catalogRevision();
    const int catalogChangesBeforeCancel = catalogSpy.count();

    manager.cancelCatalogDownload();
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(manager.constellationLineRefs().empty());

    QTRY_VERIFY(networkAccessManager.requestedUrls().size() >= 4);
    QTRY_VERIFY(allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();

    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), catalogChangesBeforeCancel);
}

void SkyCatalogManagerTests::currentConstellationResponseAppliesOnce()
{
    const QString catalogUrl = kHygPreset.urls.value(0);
    const QString constellationUrl = kHygPreset.relatedDatasetUrls.value(0);
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 900014, .properName = "Current HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        constellationUrl, {.payload = skygate::ui::tests::sampleConstellationIndexJsonPayload()}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    manager.loadCatalogPreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(manager.sourceLabel() == QStringLiteral("HYG v4.2"));
    QTRY_VERIFY(manager.constellationLineRefs().size() == 2U);

    QCOMPARE(manager.constellationLineRefs().size(), 2U);
    QCOMPARE(manager.constellationAnchorGroups().size(), 1U);
    QCOMPARE(manager.constellationCount(), 1U);

    const auto revisionAfterApply = manager.catalogRevision();
    const int catalogChangesAfterApply = catalogSpy.count();

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(manager.constellationLineRefs().size(), 2U);
    QCOMPARE(manager.constellationAnchorGroups().size(), 1U);
    QCOMPARE(manager.catalogRevision(), revisionAfterApply);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterApply);
}

void SkyCatalogManagerTests::collectionSourcesLoadEnableDisableAndRemoveIndependently()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    const QString sourceAUrl = writeSourceFile(QStringLiteral("collection-a.csv"), "Collection Star A", 901001);
    const QString sourceBUrl = writeSourceFile(QStringLiteral("collection-b.csv"), "Collection Star B", 901002);
    const QString sourceCUrl = writeSourceFile(QStringLiteral("collection-c.csv"), "Collection Star C", 901003);
    QVERIFY(!sourceAUrl.isEmpty());
    QVERIFY(!sourceBUrl.isEmpty());
    QVERIFY(!sourceCUrl.isEmpty());

    loadLocalSource(sourceAUrl);
    const QString sourceAId = manager.sourceInstanceIds().last();
    loadLocalSource(sourceBUrl);
    loadLocalSource(sourceCUrl);

    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.disableSource(sourceAId);
    QVERIFY(!manager.isSourceEnabled(sourceAId));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));

    manager.enableSource(sourceAId);
    QVERIFY(manager.isSourceEnabled(sourceAId));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));

    manager.removeSource(sourceAId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Collection Star C")));
}

void SkyCatalogManagerTests::failedCollectionLoadPreservesPriorDataAndRetrySucceeds()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString sourceAPath = m_settings.filePath(QStringLiteral("retry-source-a.csv"));
    QVERIFY(writeFile(
        sourceAPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 901010, .hip = 901010, .properName = "Retry Source A", .mag = "1.0"}
        )
    ));
    const QString sourceAUrl = QUrl::fromLocalFile(sourceAPath).toString();

    const auto loadLocalSource = [&](const QString& url) {
        manager.loadSource(
            skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url),
            skygate::ephemeris::CatalogCompositionPolicy::Merge
        );
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    loadLocalSource(sourceAUrl);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    const std::uint64_t revisionAfterA = manager.catalogRevision();

    const QString retryPath = m_settings.filePath(QStringLiteral("retry-source-b.csv"));
    const QString retryUrl = QUrl::fromLocalFile(retryPath).toString();

    // The published snapshot, its provenance, and its presentation stay
    // untouched by a failed publication: only the operation reports the error.
    const skygate::ephemeris::IStarCatalog* const catalogBeforeFailure = manager.starCatalog();
    const std::size_t bodyCountBeforeFailure = manager.bodyCount();
    const QStringList sourceIdsBeforeFailure(manager.sourceIds().begin(), manager.sourceIds().end());
    const std::vector<QStringList> contributorsBeforeFailure = manager.contributorSourceIds();
    const QString summaryBeforeFailure = manager.participationSummary();
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed file://.*/retry-source-b\\.csv .* HTTP 0")
    );
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(retryUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto retryEntry =
        std::find_if(failedView.begin(), failedView.end(), [](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.hasError;
        });
    QVERIFY(retryEntry != failedView.end());
    const QString retrySourceId = retryEntry->instanceId;

    // A failed source operation leaves the prior valid active data untouched.
    QCOMPARE(manager.catalogRevision(), revisionAfterA);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.starCatalog(), catalogBeforeFailure);
    QCOMPARE(manager.bodyCount(), bodyCountBeforeFailure);
    QCOMPARE(QStringList(manager.sourceIds().begin(), manager.sourceIds().end()), sourceIdsBeforeFailure);
    QCOMPARE(manager.contributorSourceIds().size(), contributorsBeforeFailure.size());
    for (std::size_t index = 0; index < contributorsBeforeFailure.size(); ++index) {
        QCOMPARE(manager.contributorSourceIds()[index], contributorsBeforeFailure[index]);
    }
    QCOMPARE(manager.participationSummary(), summaryBeforeFailure);
    QCOMPARE(catalogSpy.count(), 0);
    QVERIFY(manager.statusText().contains(QStringLiteral("failed")));

    QVERIFY(writeFile(
        retryPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 901011, .hip = 901011, .properName = "Retry Source B", .mag = "1.0"}
        )
    ));
    manager.retrySource(retrySourceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source B")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Source A")));
    QCOMPARE(manager.sourceCount(), std::size_t{3});
}

void SkyCatalogManagerTests::addSourceUrlAddsSameCategorySourcesWithStableIdentity()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString firstUrl = writeSourceFile(QStringLiteral("same-category-a.csv"), "Same Category A", 902001);
    const QString secondUrl = writeSourceFile(QStringLiteral("same-category-b.csv"), "Same Category B", 902002);
    QVERIFY(!firstUrl.isEmpty());
    QVERIFY(!secondUrl.isEmpty());

    manager.addSourceUrl(firstUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(secondUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Same Category A")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Same Category B")));

    QVERIFY(firstId != secondId);
    QVERIFY(manager.sourceInstanceIds().contains(firstId));
    QVERIFY(manager.sourceInstanceIds().contains(secondId));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    QCOMPARE(view.size(), 3);
    QCOMPARE(view[1].instanceId, firstId);
    QCOMPARE(view[2].instanceId, secondId);
    QCOMPARE(view[1].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(view[2].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
}

void SkyCatalogManagerTests::addSourcePresetUsesDescriptorPolicy()
{
    const QString hygUrl = kHygPreset.urls.value(0);
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        hygUrl,
        {.payload =
             skygate::ui::tests::sampleHygCsvPayload({.hip = 902010, .properName = "Preset HYG Star", .mag = "1.0"})}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0998", .ngc = "0998", .identifiers = "PGC 9998", .commonName = "Preset Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("hyg_v42"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString hygInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(hygInstanceId));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Preset HYG Star")));

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString openNgcInstanceId = manager.sourceInstanceIds().last();
    QVERIFY(manager.sourceInstanceIds().contains(openNgcInstanceId));
    QVERIFY(hygInstanceId != openNgcInstanceId);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findViewEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto hygView = findViewEntry(hygInstanceId);
    const auto openNgcView = findViewEntry(openNgcInstanceId);
    QVERIFY(hygView != view.end());
    QVERIFY(openNgcView != view.end());
    QCOMPARE(hygView->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QCOMPARE(openNgcView->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
}

void SkyCatalogManagerTests::moveSourceReordersActiveSources()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName, const int hip) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = hip, .hip = hip, .properName = QByteArray(properName), .mag = QByteArray("1.0")}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    const QString aUrl = writeSourceFile(QStringLiteral("move-a.csv"), "Move Star A", 902020);
    const QString bUrl = writeSourceFile(QStringLiteral("move-b.csv"), "Move Star B", 902021);
    QVERIFY(!aUrl.isEmpty());
    QVERIFY(!bUrl.isEmpty());

    manager.addSourceUrl(aUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString aId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(bUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString bId = manager.sourceInstanceIds().last();
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), aId, bId}));

    manager.moveSource(bId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
}

void SkyCatalogManagerTests::clearSourceCacheKeepsConfiguredRecords()
{
    const QString aPath = m_settings.filePath(QStringLiteral("clear-a.csv"));
    const QString bPath = m_settings.filePath(QStringLiteral("clear-b.csv"));
    QVERIFY(writeFile(
        aPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 902030, .hip = 902030, .properName = "Clear Star A", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        bPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 902031, .hip = 902031, .properName = "Clear Star B", .mag = "1.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    const QString aUrl = QUrl::fromLocalFile(aPath).toString();
    const QString bUrl = QUrl::fromLocalFile(bPath).toString();

    manager.addSourceUrl(aUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString aId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(bUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString bId = manager.sourceInstanceIds().last();

    const auto beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordBeforeClear =
        findCollectionRecord(beforeClear.snapshot, aId);
    QVERIFY(recordBeforeClear != nullptr);
    QVERIFY(!recordBeforeClear->payload.isEmpty());

    QVERIFY(manager.clearSourceCache(aId));

    // The accepted in-memory snapshot stays active for the session, so the
    // cleared source keeps contributing until a reload or restart.
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), aId, bId}));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Clear Star A")));

    // Only the cleared source's disposable payload is evicted; both configured
    // records stay and the cleared record keeps every configuration field.
    const auto clearedCache = store.loadCatalogCollectionCache();
    QVERIFY(clearedCache.isLoaded());
    QCOMPARE(clearedCache.snapshot.sources.size(), 3);
    const SkySettingsStore::CatalogSourceCacheRecord* clearedRecord = findCollectionRecord(clearedCache.snapshot, aId);
    QVERIFY(clearedRecord != nullptr);
    compareConfiguredSourceFields(*clearedRecord, *recordBeforeClear);
    QVERIFY(clearedRecord->payload.isEmpty());
    QVERIFY(clearedRecord->binaryPayload.isEmpty());
    QVERIFY(clearedRecord->constellationLineRows.isEmpty());
    QVERIFY(clearedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(clearedRecord->constellationCount, std::size_t{0});
    const SkySettingsStore::CatalogSourceCacheRecord* peerRecord = findCollectionRecord(clearedCache.snapshot, bId);
    QVERIFY(peerRecord != nullptr);
    QVERIFY(!peerRecord->payload.isEmpty());
    QVERIFY(!peerRecord->binaryPayload.isEmpty());

    // A metadata-only persistence of the surviving collection keeps the
    // eviction instead of re-serializing the retained in-memory payload.
    manager.moveSource(bId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
    const auto reordered = store.loadCatalogCollectionCache();
    QVERIFY(reordered.isLoaded());
    QCOMPARE(reordered.snapshot.sources.size(), 3);
    QCOMPARE(reordered.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QCOMPARE(reordered.snapshot.sources[1].instanceId, bId);
    QCOMPARE(reordered.snapshot.sources[2].instanceId, aId);
    QVERIFY(!reordered.snapshot.sources[1].payload.isEmpty());
    const SkySettingsStore::CatalogSourceCacheRecord* reorderedCleared = findCollectionRecord(reordered.snapshot, aId);
    QVERIFY(reorderedCleared != nullptr);
    SkySettingsStore::CatalogSourceCacheRecord expectedReordered = *recordBeforeClear;
    expectedReordered.order = 2;
    compareConfiguredSourceFields(*reorderedCleared, expectedReordered);
    QVERIFY(reorderedCleared->payload.isEmpty());
    QVERIFY(reorderedCleared->binaryPayload.isEmpty());

    // Restart: the evicted source stays configured and reports the unavailable
    // payload and retry state, while the peer restores its payload normally.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Saved catalog source cache has no payload")));
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId, aId}));
    QVERIFY(catalogContainsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star B")));
    QVERIFY(!catalogContainsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star A")));
    const QVector<SkyCatalogManager::SourceViewEntry> view = restarted.sourceViewEntries();
    const auto evictedEntry =
        std::find_if(view.begin(), view.end(), [&aId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == aId;
        });
    QVERIFY(evictedEntry != view.end());
    QVERIFY(evictedEntry->hasError);
    QVERIFY(!evictedEntry->busy);
    QCOMPARE(evictedEntry->statusText, QStringLiteral("Payload unavailable"));

    // An explicit successful reload repopulates the payload and restores the
    // source data; the eviction is not permanent configuration.
    restarted.retrySource(aId);
    QTRY_VERIFY(!restarted.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(restarted.starCatalog(), QStringLiteral("Clear Star A")));
    const auto recovered = store.loadCatalogCollectionCache();
    QVERIFY(recovered.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recoveredRecord = findCollectionRecord(recovered.snapshot, aId);
    QVERIFY(recoveredRecord != nullptr);
    QVERIFY(!recoveredRecord->payload.isEmpty());

    // Remove stays the separate configuration-deletion operation.
    restarted.removeSource(aId);
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), bId}));
    const auto afterRemove = store.loadCatalogCollectionCache();
    QVERIFY(afterRemove.isLoaded());
    QCOMPARE(afterRemove.snapshot.sources.size(), 2);
    QVERIFY(findCollectionRecord(afterRemove.snapshot, aId) == nullptr);
}

void SkyCatalogManagerTests::clearSourceCacheEvictsOwnedRelatedPayload()
{
    const QString sourceUrl = QStringLiteral("https://example.test/clear-related-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/clear-related-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.payload = skygate::ui::tests::stellariumConstellationIndexJsonPayload(
             {{QStringLiteral("orion"), {27989, 25336, 25930}}}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceUrl);
    source.relatedDatasetUrls = QStringList{relatedUrl};
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const auto withRelated = store.loadCatalogCollectionCache();
    QVERIFY(withRelated.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordWithRelated =
        findCollectionRecord(withRelated.snapshot, sourceId);
    QVERIFY(recordWithRelated != nullptr);
    QVERIFY(!recordWithRelated->payload.isEmpty());
    QVERIFY(!recordWithRelated->constellationLineRows.isEmpty());
    QVERIFY(!recordWithRelated->constellationAnchorGroupRows.isEmpty());

    QVERIFY(manager.clearSourceCache(sourceId));

    // The accepted snapshot keeps contributing until a reload or restart, but
    // the owned related dataset is payload of the same source and is evicted
    // together with its catalog payload.
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    const auto evicted = store.loadCatalogCollectionCache();
    QVERIFY(evicted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* evictedRecord = findCollectionRecord(evicted.snapshot, sourceId);
    QVERIFY(evictedRecord != nullptr);
    QVERIFY(evictedRecord->payload.isEmpty());
    QVERIFY(evictedRecord->binaryPayload.isEmpty());
    QVERIFY(evictedRecord->constellationLineRows.isEmpty());
    QVERIFY(evictedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(evictedRecord->constellationCount, std::size_t{0});
    QCOMPARE(evictedRecord->relatedDatasetUrls, QStringList{relatedUrl});

    // A metadata-only persistence of the collection must not write the still
    // retained in-memory related dataset back into the evicted record.
    manager.disableSource(sourceId);
    manager.enableSource(sourceId);
    const auto afterToggle = store.loadCatalogCollectionCache();
    QVERIFY(afterToggle.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* toggledRecord =
        findCollectionRecord(afterToggle.snapshot, sourceId);
    QVERIFY(toggledRecord != nullptr);
    QVERIFY(toggledRecord->enabled);
    QVERIFY(toggledRecord->payload.isEmpty());
    QVERIFY(toggledRecord->constellationLineRows.isEmpty());
    QVERIFY(toggledRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(toggledRecord->constellationCount, std::size_t{0});
}

void SkyCatalogManagerTests::relatedReplyAfterCacheEvictionStaysOutOfPersistedRecord()
{
    const QString catalogUrl = QStringLiteral("https://example.test/evict-related-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/evict-related-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    // The main load is accepted while its related reply is still pending.
    const auto beforeClear = store.loadCatalogCollectionCache();
    QVERIFY(beforeClear.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordBeforeClear =
        findCollectionRecord(beforeClear.snapshot, instanceId);
    QVERIFY(recordBeforeClear != nullptr);
    QVERIFY(!recordBeforeClear->payload.isEmpty());

    QVERIFY(manager.clearSourceCache(instanceId));
    skygate::ui::tests::FakeNetworkReply* pendingReply = findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(pendingReply != nullptr);
    QVERIFY(!pendingReply->isFinished());

    // The related download completes after the eviction. The runtime keeps the
    // dataset for the session, but persistence must not write it into the
    // evicted record.
    pendingReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const auto afterEviction = store.loadCatalogCollectionCache();
    QVERIFY(afterEviction.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* evictedRecord =
        findCollectionRecord(afterEviction.snapshot, instanceId);
    QVERIFY(evictedRecord != nullptr);
    QVERIFY(evictedRecord->payload.isEmpty());
    QVERIFY(evictedRecord->constellationLineRows.isEmpty());
    QVERIFY(evictedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(evictedRecord->constellationCount, std::size_t{0});

    // Restart: the source stays configured and reports the unavailable payload
    // state; the related data that landed after the eviction did not sneak
    // back into the record.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Saved catalog source cache has no payload")));
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    const QVector<SkyCatalogManager::SourceViewEntry> view = restarted.sourceViewEntries();
    QCOMPARE(view.size(), std::size_t{2});
    const auto evictedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(evictedEntry != view.end());
    QVERIFY(evictedEntry->hasError);
    QVERIFY(!evictedEntry->busy);
    QCOMPARE(evictedEntry->statusText, QStringLiteral("Payload unavailable"));
}

void SkyCatalogManagerTests::sameDescriptorInstancesCoexistIndependently()
{
    const std::optional<skygate::ui::internal::SkyCatalogSourceDescriptor> openNgcPreset =
        skygate::ui::internal::SkyCatalogPresets::deepSkySourceDescriptor(QStringLiteral("open_ngc"));
    QVERIFY(openNgcPreset.has_value());
    const QString openNgcUrl = openNgcPreset->urls.value(0);

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0991",
              .type = "G",
              .ra = "00:20:00.00",
              .dec = "+10:00:00.0",
              .messier = "",
              .ngc = "0991",
              .identifiers = "PGC 9991",
              .commonName = "First Galaxy"}
         )}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0992",
              .type = "G",
              .ra = "00:30:00.00",
              .dec = "+11:00:00.0",
              .messier = "",
              .ngc = "0992",
              .identifiers = "PGC 9992",
              .commonName = "Second Galaxy"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString firstId = manager.sourceInstanceIds().last();
    manager.addSourcePreset(QStringLiteral("open_ngc"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString secondId = manager.sourceInstanceIds().last();

    QVERIFY(firstId != secondId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance participates independently.
    manager.disableSource(firstId);
    QVERIFY(!manager.isSourceEnabled(firstId));
    QVERIFY(manager.isSourceEnabled(secondId));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    manager.enableSource(firstId);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reloads independently.
    networkAccessManager.enqueueResponse(
        openNgcUrl,
        {.payload = skygate::ui::tests::sampleOpenNgcCsvPayload(
             {.name = "NGC0993",
              .type = "G",
              .ra = "00:35:00.00",
              .dec = "+11:30:00.0",
              .messier = "",
              .ngc = "0993",
              .identifiers = "PGC 9993",
              .commonName = "Reloaded Galaxy"}
         )}
    );
    manager.retrySource(firstId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), firstId, secondId}));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 991")));

    // Each instance reorders independently.
    manager.moveSource(secondId, 1);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));

    // Restart restores the bundled source and both instances with their
    // durable IDs and order.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId, firstId}));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));

    // Each instance removes independently.
    restoredManager.removeSource(firstId);
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), secondId}));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 992")));
    QVERIFY(!catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("NGC 993")));
}

void SkyCatalogManagerTests::sameUrlDifferentVersionsStayDistinct()
{
    const QString catalogPath = m_settings.filePath(QStringLiteral("versioned-star.csv"));
    QVERIFY(writeFile(
        catalogPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903001, .hip = 903001, .properName = "Versioned Star", .mag = "1.0"}
        )
    ));
    const QString catalogUrl = QUrl::fromLocalFile(catalogPath).toString();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance first =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v1"));
    first.archiveSelector = QStringLiteral("members/catalog-v1.csv");
    skygate::ui::internal::SkyCatalogSourceInstance second =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl, QStringLiteral("v2"));
    second.archiveSelector = QStringLiteral("members/catalog-v2.csv");

    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(first.instanceId != second.instanceId);
    QCOMPARE(manager.sourceCount(), std::size_t{3});

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto findEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto firstEntry = findEntry(first.instanceId);
    const auto secondEntry = findEntry(second.instanceId);
    QVERIFY(firstEntry != view.end());
    QVERIFY(secondEntry != view.end());
    QCOMPARE(firstEntry->version, QStringLiteral("v1"));
    QCOMPARE(secondEntry->version, QStringLiteral("v2"));

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 3);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[1].instanceId, first.instanceId);
    QCOMPARE(snapshot.snapshot.sources[1].version, QStringLiteral("v1"));
    QCOMPARE(snapshot.snapshot.sources[1].archiveSelector, QStringLiteral("members/catalog-v1.csv"));
    QCOMPARE(snapshot.snapshot.sources[2].instanceId, second.instanceId);
    QCOMPARE(snapshot.snapshot.sources[2].version, QStringLiteral("v2"));
    QCOMPARE(snapshot.snapshot.sources[2].archiveSelector, QStringLiteral("members/catalog-v2.csv"));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), first.instanceId, second.instanceId})
    );
}

void SkyCatalogManagerTests::editingSourceAsUpdatePreservesInstanceId()
{
    const QString firstPath = m_settings.filePath(QStringLiteral("edit-source-v1.csv"));
    QVERIFY(writeFile(
        firstPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903010, .hip = 903010, .properName = "Edit Star V1", .mag = "1.0"}
        )
    ));
    const QString secondPath = m_settings.filePath(QStringLiteral("edit-source-v2.csv"));
    QVERIFY(writeFile(
        secondPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 903011, .hip = 903011, .properName = "Edit Star V2", .mag = "2.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(
            QUrl::fromLocalFile(firstPath).toString(), QStringLiteral("v1")
        );
    source.title = QStringLiteral("Original Title");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString instanceId = source.instanceId;
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    // Editing title, URL, and version updates the same configured instance
    // instead of adding another one with a new ID.
    source.title = QStringLiteral("Updated Title");
    source.urls = QStringList{QUrl::fromLocalFile(secondPath).toString()};
    source.version = QStringLiteral("v2");
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V2")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Edit Star V1")));

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto entry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& candidate) {
            return candidate.instanceId == instanceId;
        });
    QVERIFY(entry != view.end());
    QCOMPARE(entry->title, QStringLiteral("Updated Title"));
    QCOMPARE(entry->version, QStringLiteral("v2"));

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 2);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, QStringLiteral("primary"));
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[1].instanceId, instanceId);
    QCOMPARE(snapshot.snapshot.sources[1].version, QStringLiteral("v2"));
}

// An update attempt that never becomes active must not become the source's
// configurable facts: the still-active source keeps its accepted descriptor,
// parse contract, and payload, and ordinary persistence serializes only those.
void SkyCatalogManagerTests::failedUpdateKeepsAcceptedSourceFactsWhenPersisting()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/accepted-archive.zip");
    const QString rejectedUrl = QStringLiteral("https://example.test/rejected-archive.zip");
    const QString unreachableUrl = QStringLiteral("https://example.test/unreachable-archive.zip");
    const QByteArray acceptedPayload = archiveMemberZip(
        {.name = "NGC0991", .messier = "", .ngc = "0991", .identifiers = "PGC 9991", .commonName = "Accepted Member"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Accepted V1 Title");
    accepted.descriptorId = QStringLiteral("accepted_descriptor");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    accepted.archiveSelector = QString::fromLatin1(kArchiveStarsMember);
    accepted.attribution = QStringLiteral("Accepted attribution");
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));
    QVERIFY(!catalogContainsId(manager.starCatalog(), QStringLiteral("ngc_991")));

    // The same instance is asked to update itself to a different archive
    // member, schema hint, version, title, attribution, and related dataset.
    // The download fails, so none of it may be remembered as accepted.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Rejected V2 Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{rejectedUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    rejected.archiveSelector = QString::fromLatin1(kArchiveDeepSkyMember);
    rejected.attribution = QStringLiteral("Rejected attribution");
    rejected.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/rejected-lines.json")};

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/rejected-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(!failedEntry->busy);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("failed")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Accepted V1 Title"));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));

    // Reordering the still-active source persists the accepted facts alone.
    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Accepted V1 Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->descriptorId, QStringLiteral("accepted_descriptor"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(record->archiveSelector, QString::fromLatin1(kArchiveStarsMember));
    QCOMPARE(record->attribution, QStringLiteral("Accepted attribution"));
    QVERIFY(record->relatedDatasetUrls.isEmpty());
    QCOMPARE(record->payload, acceptedPayload);

    // A second attempt against a URL the network cannot reach keeps the same
    // boundary: the operation reports the failure and persistence still
    // serializes the accepted facts.
    rejected.urls = QStringList{unreachableUrl};
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/unreachable-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.statusText().contains(QStringLiteral("failed"), Qt::CaseInsensitive));

    manager.moveSource(instanceId, 1);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persistedAfterRetry = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterRetry.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordAfterRetry =
        findCollectionRecord(persistedAfterRetry.snapshot, instanceId);
    QVERIFY(recordAfterRetry != nullptr);
    QCOMPARE(recordAfterRetry->title, QStringLiteral("Accepted V1 Title"));
    QCOMPARE(recordAfterRetry->version, QStringLiteral("v1"));
    QCOMPARE(recordAfterRetry->urls, QStringList{acceptedUrl});
    QCOMPARE(recordAfterRetry->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(recordAfterRetry->archiveSelector, QString::fromLatin1(kArchiveStarsMember));
    QVERIFY(recordAfterRetry->relatedDatasetUrls.isEmpty());
    QCOMPARE(recordAfterRetry->payload, acceptedPayload);
}

void SkyCatalogManagerTests::acceptedSourceFactsSurviveRestartAfterFailedUpdate()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("failed-update-restart-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString acceptedUrl = QStringLiteral("https://example.test/restart-accepted-archive.zip");
    const QString rejectedUrl = QStringLiteral("https://example.test/restart-rejected-archive.zip");
    const QByteArray acceptedPayload = archiveMemberZip(
        {.name = "NGC0992", .messier = "", .ngc = "0992", .identifiers = "PGC 9992", .commonName = "Restart Accepted"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Restart Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    accepted.archiveSelector = QString::fromLatin1(kArchiveStarsMember);
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Archive Member Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Restart Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{rejectedUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    rejected.archiveSelector = QString::fromLatin1(kArchiveDeepSkyMember);

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/restart-rejected-archive\\.zip .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The reorder writes the collection snapshot the restarts read back.
    manager.moveSource(instanceId, 0);

    // A restart from the binary cache rebuilds exactly the accepted source.
    {
        SkyCatalogManager binaryRestart(&store, nullptr, nullptr, &networkAccessManager);
        QVERIFY(binaryRestart.restoreCatalogCache());
        QCOMPARE(binaryRestart.sourceInstanceIds().count(instanceId), 1);
        QVERIFY(catalogContainsDisplayName(binaryRestart.starCatalog(), QStringLiteral("Archive Member Star")));
        QVERIFY(!catalogContainsId(binaryRestart.starCatalog(), QStringLiteral("ngc_992")));
        QCOMPARE(
            durableTitle(binaryRestart.sourceTitles().value(instanceId)), QStringLiteral("Restart Accepted Title")
        );
    }

    // Losing the binary sidecar must not make the raw fallback parse the
    // accepted bytes with the rejected attempt's member selection and hint.
    const QString binaryPath = catalogSourceSidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    QVERIFY(!binaryPath.isEmpty());
    QVERIFY(QFile::remove(binaryPath));

    {
        SkyCatalogManager rawRestart(&store, nullptr, nullptr, &networkAccessManager);
        QVERIFY(rawRestart.restoreCatalogCache());
        QCOMPARE(rawRestart.sourceInstanceIds().count(instanceId), 1);
        QVERIFY(catalogContainsDisplayName(rawRestart.starCatalog(), QStringLiteral("Archive Member Star")));
        QVERIFY(!catalogContainsId(rawRestart.starCatalog(), QStringLiteral("ngc_992")));
        QCOMPARE(durableTitle(rawRestart.sourceTitles().value(instanceId)), QStringLiteral("Restart Accepted Title"));

        const SkySettingsStore::CatalogCollectionCacheLoadResult upgraded = store.loadCatalogCollectionCache();
        QVERIFY(upgraded.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* upgradedRecord =
            findCollectionRecord(upgraded.snapshot, instanceId);
        QVERIFY(upgradedRecord != nullptr);
        QCOMPARE(upgradedRecord->version, QStringLiteral("v1"));
        QCOMPARE(upgradedRecord->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
        QCOMPARE(upgradedRecord->archiveSelector, QString::fromLatin1(kArchiveStarsMember));
        QCOMPARE(upgradedRecord->payload, acceptedPayload);
    }
}

void SkyCatalogManagerTests::successfulRetryInstallsUpdateFactsTogether()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/retry-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/retry-update.csv");
    const QByteArray updatePayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0998", .messier = "", .ngc = "0998", .identifiers = "PGC 9998", .commonName = "Retried Galaxy"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908001, .hip = 908001, .properName = "Retry Accepted Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Retry Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Accepted Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Retried Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Catalog source failed https://example\\.test/retry-update\\.csv .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Persisting while the attempt is still only an attempt serializes the
    // accepted facts; the successful retry below then replaces them together.
    manager.moveSource(instanceId, 0);
    {
        const SkySettingsStore::CatalogCollectionCacheLoadResult persistedBeforeRetry =
            store.loadCatalogCollectionCache();
        QVERIFY(persistedBeforeRetry.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* recordBeforeRetry =
            findCollectionRecord(persistedBeforeRetry.snapshot, instanceId);
        QVERIFY(recordBeforeRetry != nullptr);
        QCOMPARE(recordBeforeRetry->urls, QStringList{acceptedUrl});
        QCOMPARE(recordBeforeRetry->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    }

    // The failed attempt stays retryable and its success commits the new
    // metadata and the data it was activated with in one step, under the same
    // durable instance ID.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(catalogContainsId(manager.starCatalog(), QStringLiteral("ngc_998")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Retry Accepted Star")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Retried Title"));

    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Retried Title"));
    QCOMPARE(record->version, QStringLiteral("v2"));
    QCOMPARE(record->urls, QStringList{updateUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(record->payload, updatePayload);

    SkyCatalogManager restarted(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds().count(instanceId), 1);
    QVERIFY(catalogContainsId(restarted.starCatalog(), QStringLiteral("ngc_998")));
    QVERIFY(!catalogContainsDisplayName(restarted.starCatalog(), QStringLiteral("Retry Accepted Star")));
    QCOMPARE(durableTitle(restarted.sourceTitles().value(instanceId)), QStringLiteral("Retried Title"));
}

void SkyCatalogManagerTests::canceledUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/cancel-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/cancel-update.csv");
    const QByteArray updatePayload = skygate::ui::tests::sampleOpenNgcCsvPayload(
        {.name = "NGC0997", .messier = "", .ngc = "0997", .identifiers = "PGC 9997", .commonName = "Canceled Galaxy"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908010, .hip = 908010, .properName = "Cancel Accepted Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Cancel Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));

    // The update download stays open until the test cancels it.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload, .manualFinish = true});
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Canceled Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(manager.downloadingCatalog());

    manager.cancelCatalogDownload();
    QVERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> canceledView = manager.sourceViewEntries();
    const auto canceledEntry = std::find_if(
        canceledView.begin(), canceledView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(canceledEntry != canceledView.end());
    QVERIFY(!canceledEntry->busy);
    QVERIFY(!canceledEntry->hasError);
    QCOMPARE(canceledEntry->statusText, QStringLiteral("Canceled"));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));

    // Persisting after the cancellation still serializes the accepted facts.
    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Cancel Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);

    // The canceled attempt is still the configuration a retry repeats.
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsId(manager.starCatalog(), QStringLiteral("ngc_997")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Cancel Accepted Star")));
    QCOMPARE(manager.sourceTitles().value(instanceId), QStringLiteral("Canceled Title"));
}

void SkyCatalogManagerTests::parseFailedUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/parse-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/parse-failed-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        acceptedUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 908020, .hip = 908020, .properName = "Parse Accepted Star", .mag = "1.0"}
         )}
    );
    networkAccessManager.enqueueResponse(updateUrl, {.payload = "not a catalog payload\n"});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.title = QStringLiteral("Parse Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Parse Accepted Star")));

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Parse Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog: Source https://example\\.test/parse-failed-update\\.csv parse failed")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("parse failed")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Parse Accepted Star")));

    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Parse Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(
        record->payload,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 908020, .hip = 908020, .properName = "Parse Accepted Star", .mag = "1.0"}
        )
    );
}

void SkyCatalogManagerTests::rejectedCompositionAfterParsedUpdateKeepsAcceptedSourceFacts()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/composition-accepted.csv");
    const QString updateUrl = QStringLiteral("https://example.test/composition-update.csv");
    const QByteArray acceptedPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908030, .hip = 908030, .properName = "Composition Accepted Star", .mag = "1.0"}
    );
    const QByteArray updatePayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 908031, .hip = 908031, .properName = "Composition Parsed Star", .mag = "2.0"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    // The bundled deep-sky fallback is disabled while the source loads, so its
    // reserved identity is available to the configured source. Re-enabling the
    // fallback later makes any transition of that source collide with it.
    manager.setDeepSkyCatalogPresetIndex(1);

    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(acceptedUrl, QStringLiteral("v1"));
    accepted.instanceId = QStringLiteral("bundled-deep-sky");
    accepted.title = QStringLiteral("Composition Accepted Title");
    accepted.schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv;
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Composition Accepted Star")));

    manager.setDeepSkyCatalogPresetIndex(0);

    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Composition Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The catalog parsed but the runtime refused the collection transition:
    // the operation reports it and the accepted snapshot stays active.
    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("rejected")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Composition Accepted Star")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Composition Parsed Star")));

    // Persisting later serializes the accepted facts, not the parsed attempt.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Composition Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->schemaHint, skygate::ephemeris::CatalogSourceType::HygCsv);
    QCOMPARE(record->payload, acceptedPayload);
}

void SkyCatalogManagerTests::failedUpdateSupersedesItsPendingRelatedResponse()
{
    const QString acceptedUrl = QStringLiteral("https://example.test/superseded-accepted.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/superseded-accepted-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/superseded-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance accepted = relatedDatasetInstance(acceptedUrl, relatedUrl);
    accepted.title = QStringLiteral("Superseded Accepted Title");
    accepted.version = QStringLiteral("v1");
    const QString instanceId = accepted.instanceId;

    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    // The update attempt fails, which supersedes the pending related reply of
    // the previous incarnation while the accepted catalog stays active.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Superseded Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/superseded-rejected-lines.json")};

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/superseded-update\\.csv .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QString statusAfterFailure = manager.statusText();
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The superseded reply cannot attach its data to the still-accepted source
    // or overwrite its operation error.
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.statusText(), statusAfterFailure);

    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->title, QStringLiteral("Superseded Accepted Title"));
    QCOMPARE(record->version, QStringLiteral("v1"));
    QCOMPARE(record->urls, QStringList{acceptedUrl});
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(record->constellationLineRows.isEmpty());
}

void SkyCatalogManagerTests::restoresLegacyPersistedInstanceIdsWithReferences()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));

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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Legacy Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({legacyInstanceId}));

    // Restart keeps the same legacy ID.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({legacyInstanceId}));
}

void SkyCatalogManagerTests::legacyMigrationRenamesDuplicateInstanceIds()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Slot Star")));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("NGC 995")));

    // The renamed IDs stay stable through the migration persist and restart.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), restoredIds);
}

void SkyCatalogManagerTests::restoresArchiveSelectionAndSourceMetadataAfterBinaryCacheLoss()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("archive-restore-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString archiveUrl = QStringLiteral("https://example.test/members.zip");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = archiveMemberZip(
             {.name = "NGC0991", .messier = "", .ngc = "0991", .identifiers = "PGC 9991", .commonName = "First"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(archiveUrl, QStringLiteral("v2026.1"));
    const QString instanceId = source.instanceId;
    source.descriptorId = QStringLiteral("archive_demo");
    source.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    source.archiveSelector = QString::fromLatin1(kArchiveDeepSkyMember);
    source.attribution = QStringLiteral("Demo archive attribution");

    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsId(manager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(manager.starCatalog(), QStringLiteral("hip_900101")));

    // The parse contract and descriptor metadata are persisted with the source.
    // The default bundled source is persisted as configuration without a
    // payload so its position survives the restart.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QCOMPARE(persisted.snapshot.sources.size(), 2);
    const auto persistedSource = std::find_if(
        persisted.snapshot.sources.begin(),
        persisted.snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(persistedSource != persisted.snapshot.sources.end());
    QCOMPARE(persistedSource->descriptorId, QString("archive_demo"));
    QCOMPARE(persistedSource->version, QString("v2026.1"));
    QCOMPARE(persistedSource->archiveSelector, QString::fromLatin1(kArchiveDeepSkyMember));
    QCOMPARE(persistedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(persistedSource->attribution, QString("Demo archive attribution"));

    // Losing the binary sidecar must not lose the selected archive member.
    const QDir cacheDir(cacheDirectory);
    const QStringList binaryFiles =
        cacheDir.entryList(QStringList{QStringLiteral("catalog-source-*.bin")}, QDir::Files);
    QCOMPARE(binaryFiles.size(), 1);
    const QString binaryPath = catalogSourceSidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    QCOMPARE(binaryFiles.first(), QFileInfo(binaryPath).fileName());
    QVERIFY(QFile::remove(binaryPath));

    SkyCatalogManager restoredManager(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    QVERIFY(catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));

    // The raw-payload fallback rewrote the upgraded record with the same parse
    // contract, and a reload keeps replaying the restored selection.
    const auto upgraded = store.loadCatalogCollectionCache();
    QVERIFY(upgraded.isLoaded());
    QCOMPARE(upgraded.snapshot.sources.size(), 2);
    const auto upgradedSource = std::find_if(
        upgraded.snapshot.sources.begin(),
        upgraded.snapshot.sources.end(),
        [&instanceId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == instanceId;
        }
    );
    QVERIFY(upgradedSource != upgraded.snapshot.sources.end());
    QCOMPARE(upgradedSource->archiveSelector, QString::fromLatin1(kArchiveDeepSkyMember));
    QCOMPARE(upgradedSource->schemaHint, skygate::ephemeris::CatalogSourceType::OpenNgcCsv);
    QCOMPARE(upgradedSource->attribution, QString("Demo archive attribution"));

    networkAccessManager.enqueueResponse(
        archiveUrl,
        {.payload = archiveMemberZip(
             {.name = "NGC0993", .messier = "", .ngc = "0993", .identifiers = "PGC 9993", .commonName = "Reloaded"}
         )}
    );
    restoredManager.retrySource(instanceId);
    QTRY_VERIFY(!restoredManager.downloadingCatalog());
    QVERIFY(catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_993")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("ngc_991")));
    QVERIFY(!catalogContainsId(restoredManager.starCatalog(), QStringLiteral("hip_900101")));
}

void SkyCatalogManagerTests::legacyBundledStarSlotSurvivesMigrationThroughManager()
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

void SkyCatalogManagerTests::persistsBundledSourceConfigurationWithoutNetworkOperation()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});
    const QString bundledId = manager.sourceInstanceIds().first();
    QCOMPARE(bundledId, QStringLiteral("primary"));
    QVERIFY(manager.isSourceEnabled(bundledId));

    // Disabling the bundled source persists its configuration alone; no URL,
    // download, or operation record is involved.
    manager.disableSource(bundledId);

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 1);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, bundledId);
    QVERIFY(snapshot.snapshot.sources[0].bundled);
    QCOMPARE(snapshot.snapshot.sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!snapshot.snapshot.sources[0].enabled);
    QVERIFY(snapshot.snapshot.sources[0].urls.isEmpty());
    QVERIFY(snapshot.snapshot.sources[0].payload.isEmpty());
    QVERIFY(snapshot.snapshot.sources[0].binaryPayload.isEmpty());

    // Restart rebuilds the bundled source from the factory and keeps it
    // disabled instead of silently enabling it again.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{bundledId});
    QVERIFY(!restoredManager.isSourceEnabled(bundledId));
    QVERIFY(restoredManager.starCatalog() != nullptr);
    const auto composedIds = restoredManager.sourceIds();
    QVERIFY(std::none_of(composedIds.begin(), composedIds.end(), [&bundledId](const QString& sourceId) {
        return sourceId == bundledId;
    }));

    const QVector<SkyCatalogManager::SourceViewEntry> view = restoredManager.sourceViewEntries();
    QCOMPARE(view.size(), 1);
    QCOMPARE(view[0].instanceId, bundledId);
    QCOMPARE(view[0].title, QStringLiteral("Bundled"));
    QVERIFY(view[0].bundled);
    QVERIFY(!view[0].enabled);
}

void SkyCatalogManagerTests::restoredBundledDeepSkySourceKeepsFreshObjectCount()
{
    SkySettingsStore store;
    QString freshInfoText;
    {
        SkyCatalogManager manager(&store);
        manager.addSourcePreset(QStringLiteral("bundled_messier"));
        freshInfoText = manager.deepSkyCatalogInfoText();
        QVERIFY(freshInfoText.contains(QStringLiteral("Objects:")));
    }

    // A restart reports the same deep-sky object count as the fresh add: the
    // restored bundled record leaves the count to the bundled fallback
    // participation instead of reporting the rebuilt catalog's deep-sky
    // objects a second time.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.deepSkyCatalogInfoText(), freshInfoText);
}

void SkyCatalogManagerTests::interleavedBundledAndDownloadedSourcesPreserveOrderAndPrecedence()
{
    const auto writeSourceFile = [&](const QString& fileName, const char* properName) -> QString {
        const QString path = m_settings.filePath(fileName);
        if (!writeFile(
                path,
                skygate::ui::tests::sampleHygCsvPayload(
                    {.id = 906001, .hip = 906001, .properName = QByteArray(properName), .mag = "1.0"}
                )
            )) {
            return QString();
        }
        return QUrl::fromLocalFile(path).toString();
    };

    const QString sourceAUrl = writeSourceFile(QStringLiteral("interleave-a.csv"), "Interleave Star A");
    const QString sourceBUrl = writeSourceFile(QStringLiteral("interleave-b.csv"), "Interleave Star B");
    QVERIFY(!sourceAUrl.isEmpty());
    QVERIFY(!sourceBUrl.isEmpty());

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    manager.addSourceUrl(sourceAUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString sourceAId = manager.sourceInstanceIds().last();
    manager.addSourceUrl(sourceBUrl, QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString sourceBId = manager.sourceInstanceIds().last();
    manager.addSourcePreset(QStringLiteral("bundled_messier"));
    const QString bundledDeepSkyId = manager.sourceInstanceIds().last();
    QCOMPARE(manager.sourceCount(), std::size_t{4});

    // Interleave the bundled source between the two downloaded sources so the
    // collection is neither bundled-first nor bundled-last.
    manager.moveSource(QStringLiteral("primary"), 1);
    QCOMPARE(
        manager.sourceInstanceIds(), QStringList({sourceAId, QStringLiteral("primary"), sourceBId, bundledDeepSkyId})
    );
    manager.disableSource(bundledDeepSkyId);

    // The later Merge source keeps its precedence for the shared identity.
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Interleave Star A")));

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({sourceAId, QStringLiteral("primary"), sourceBId, bundledDeepSkyId})
    );

    const QVector<SkyCatalogManager::SourceViewEntry> view = restoredManager.sourceViewEntries();
    const auto findEntry = [&view](const QString& instanceId) {
        return std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    };
    const auto sourceAEntry = findEntry(sourceAId);
    const auto bundledEntry = findEntry(QStringLiteral("primary"));
    const auto sourceBEntry = findEntry(sourceBId);
    const auto bundledDeepSkyEntry = findEntry(bundledDeepSkyId);
    QVERIFY(sourceAEntry != view.end());
    QVERIFY(bundledEntry != view.end());
    QVERIFY(sourceBEntry != view.end());
    QVERIFY(bundledDeepSkyEntry != view.end());
    QCOMPARE(sourceAEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!sourceAEntry->bundled);
    QVERIFY(sourceAEntry->enabled);
    QCOMPARE(bundledEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(bundledEntry->bundled);
    QVERIFY(bundledEntry->enabled);
    QCOMPARE(bundledEntry->title, QStringLiteral("Bundled"));
    QCOMPARE(sourceBEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(!sourceBEntry->bundled);
    QVERIFY(sourceBEntry->enabled);
    QCOMPARE(bundledDeepSkyEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(bundledDeepSkyEntry->bundled);
    QVERIFY(!bundledDeepSkyEntry->enabled);
    QCOMPARE(bundledDeepSkyEntry->title, QStringLiteral("Bundled Messier"));

    // Restored order keeps the resulting precedence.
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star B")));
    QVERIFY(!catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Interleave Star A")));
}

void SkyCatalogManagerTests::unreadablePayloadKeepsConfiguredSourceWithoutErasingSiblings()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Healthy Star")));

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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Recovered Star")));

    // A further restart keeps all three configured sources.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(
        restoredManager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), QStringLiteral("custom:damaged"), QStringLiteral("custom:healthy")})
    );
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Recovered Star")));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Healthy Star")));
}

void SkyCatalogManagerTests::rejectedRestoreKeepsPreviousCollectionAndReportsError()
{
    // A persisted instance identity colliding with the implicit bundled core
    // source makes the restored collection uncomposable. The rejected restore
    // must report the operation error and keep the previous configuration,
    // snapshot, source rows, and stored cache instead of half-applying the
    // staged collection.
    const QString collidingUrl = QStringLiteral("https://example.test/colliding-stars.csv");

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;

    SkySettingsStore::CatalogSourceCacheRecord colliding;
    colliding.instanceId = QStringLiteral("bundled-core");
    colliding.title = QStringLiteral("Colliding Source");
    colliding.urls = QStringList{collidingUrl};
    colliding.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    colliding.enabled = true;
    colliding.order = 0;
    colliding.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Colliding Star", .mag = "1.0"}
    );
    snapshot.sources.push_back(std::move(colliding));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    SkyCatalogManager manager(&store);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const QStringList sourcesBefore = manager.sourceInstanceIds();
    const std::size_t bodyCountBefore = manager.bodyCount();
    const std::uint64_t revisionBefore = manager.catalogRevision();
    const int sourceRowCountBefore = manager.sourceViewEntries().size();

    QVERIFY(!manager.restoreCatalogCache());

    // The operation error is reported through the normal status signal, and
    // the failed transition publishes no catalog or dataset change.
    QCOMPARE(statusSpy.count(), 1);
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Collection rejected")));
    QCOMPARE(catalogSpy.count(), 0);
    QCOMPARE(datasetSpy.count(), 0);

    QCOMPARE(manager.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(manager.bodyCount(), bodyCountBefore);
    QCOMPARE(manager.catalogRevision(), revisionBefore);
    QVERIFY(manager.starCatalog() != nullptr);

    // No staged row appears for the rejected collection.
    const QVector<SkyCatalogManager::SourceViewEntry> rows = manager.sourceViewEntries();
    QCOMPARE(rows.size(), sourceRowCountBefore);
    QCOMPARE(rows.first().instanceId, QStringLiteral("primary"));
    QVERIFY(!rows.first().hasError);

    // The rejected collection is not persisted in place of the stored cache.
    const auto persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(collectionContainsInstanceId(persisted.snapshot, QStringLiteral("bundled-core")));
    QVERIFY(!collectionContainsInstanceId(persisted.snapshot, QStringLiteral("primary")));
}

void SkyCatalogManagerTests::relatedConstellationDatasetsStayOwnedByTheirSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/related-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/related-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/related-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/related-b-lines.json");

    const QByteArray sourceARelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("orion"), {27989, 25336, 25930}}});
    const QByteArray sourceBRelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("lyra"), {26311, 26727, 24436}}});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = sourceARelatedPayload});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = sourceBRelatedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const auto loadRelatedSource = [&](const skygate::ui::internal::SkyCatalogSourceInstance& instance) {
        manager.loadSource(instance, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceAUrl);
    sourceA.relatedDatasetUrls = QStringList{sourceARelatedUrl};
    const QString sourceAId = sourceA.instanceId;
    loadRelatedSource(sourceA);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceBUrl);
    sourceB.relatedDatasetUrls = QStringList{sourceBRelatedUrl};
    const QString sourceBId = sourceB.instanceId;
    loadRelatedSource(sourceB);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // Two loaded sources retain distinct related datasets simultaneously.
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // Disabling one source removes only its owned dataset from the active
    // view, including its anchors and its count contribution.
    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // Re-enabling restores the retained dataset.
    manager.enableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Removing one source does not remove the other source's dataset, and the
    // surviving references still resolve against the active object identities.
    manager.removeSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.resolvedConstellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.resolvedConstellationAnchorGroups().size(), std::size_t{1});
    QVERIFY(manager.isSourceEnabled(sourceBId));
}

void SkyCatalogManagerTests::overlappingConstellationDatasetsFollowSourceOrder()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/overlap-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/overlap-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/overlap-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/overlap-b-lines.json");

    // Source A declares three constellations but only two extract anchor
    // groups, so the declared count and the composed group count differ.
    const QByteArray sourceARelatedPayload = skygate::ui::tests::stellariumConstellationIndexJsonPayload(
        {{QStringLiteral("orion"), {27989, 25336, 25930}},
         {QStringLiteral("cepheus"), {26311, 26727, 24436}},
         {QStringLiteral("draco"), {42}}}
    );
    const QByteArray sourceBRelatedPayload =
        skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("orion"), {24436, 25930, 26727}}});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = sourceARelatedPayload});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = sourceBRelatedPayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const auto loadRelatedSource = [&](const skygate::ui::internal::SkyCatalogSourceInstance& instance) {
        manager.loadSource(instance, skygate::ephemeris::CatalogCompositionPolicy::Merge);
        QTRY_VERIFY(!manager.downloadingCatalog());
    };

    skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceAUrl);
    sourceA.relatedDatasetUrls = QStringList{sourceARelatedUrl};
    const QString sourceAId = sourceA.instanceId;
    loadRelatedSource(sourceA);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{3});

    skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(sourceBUrl);
    sourceB.relatedDatasetUrls = QStringList{sourceBRelatedUrl};
    loadRelatedSource(sourceB);
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{6});

    // The later enabled source owns the overlapping constellation name and the
    // declared count follows the last enabled owner that declares one.
    const auto* orionGroup = findConstellationAnchorGroup(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_24436"));
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Reordering the collection changes both the winning anchors and the
    // composed declared count.
    manager.moveSource(sourceAId, 2);
    orionGroup = findConstellationAnchorGroup(manager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.front(), std::string("hip_25336"));
    QCOMPARE(manager.constellationCount(), std::size_t{3});
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{6});
}

void SkyCatalogManagerTests::lateRelatedResponseAfterOwnerRemovalIsIgnored()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);

    manager.removeSource(sourceAId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceAId));
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    const auto revisionAfterRemoval = manager.catalogRevision();
    const int catalogChangesAfterRemoval = catalogSpy.count();
    const QString statusAfterRemoval = manager.statusText();
    const int statusChangesAfterRemoval = statusSpy.count();

    const auto entriesAfterRemoval = manager.sourceViewEntries();
    const auto siblingAfterRemoval = std::find_if(
        entriesAfterRemoval.begin(),
        entriesAfterRemoval.end(),
        [&sourceBId](const SkyCatalogManager::SourceViewEntry& entry) { return entry.instanceId == sourceBId; }
    );
    QVERIFY(siblingAfterRemoval != entriesAfterRemoval.end());
    QVERIFY(!siblingAfterRemoval->busy);
    QVERIFY(!siblingAfterRemoval->hasError);
    QCOMPARE(siblingAfterRemoval->statusText, QStringLiteral("Active"));

    const auto persistedAfterRemoval = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterRemoval.isLoaded());
    QVERIFY(!collectionContainsInstanceId(persistedAfterRemoval.snapshot, sourceAId));

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The removed owner's delayed response is discarded: no active lines or
    // counts, no catalog change, no status change, and no persisted owner data.
    // The retained sibling keeps its own dataset and status.
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(manager.catalogRevision(), revisionAfterRemoval);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterRemoval);
    QCOMPARE(manager.statusText(), statusAfterRemoval);
    QCOMPARE(statusSpy.count(), statusChangesAfterRemoval);

    const auto entriesAfterReply = manager.sourceViewEntries();
    const auto siblingAfterReply = std::find_if(
        entriesAfterReply.begin(),
        entriesAfterReply.end(),
        [&sourceBId](const SkyCatalogManager::SourceViewEntry& entry) { return entry.instanceId == sourceBId; }
    );
    QVERIFY(siblingAfterReply != entriesAfterReply.end());
    QVERIFY(!siblingAfterReply->busy);
    QVERIFY(!siblingAfterReply->hasError);
    QCOMPARE(siblingAfterReply->statusText, QStringLiteral("Active"));

    const auto persistedAfterReply = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterReply.isLoaded());
    QCOMPARE(persistedAfterReply.snapshot.sources.size(), persistedAfterRemoval.snapshot.sources.size());
    QVERIFY(!collectionContainsInstanceId(persistedAfterReply.snapshot, sourceAId));
}

void SkyCatalogManagerTests::readdedOwnerRejectsPreviousIncarnationResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/readded-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/readded-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = lyraRelatedDatasetPayload(), .manualFinish = true});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance first = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = first.instanceId;
    manager.loadSource(first, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> staleReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!staleReply.isNull());
    QVERIFY(!staleReply->isFinished());

    // Replace the owner with a new incarnation of the same durable instance
    // ID; only this incarnation's revision may accept the related response.
    manager.removeSource(sourceId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceId));

    skygate::ui::internal::SkyCatalogSourceInstance second = relatedDatasetInstance(catalogUrl, relatedUrl);
    second.instanceId = sourceId;
    manager.loadSource(second, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    const auto revisionAfterCurrent = manager.catalogRevision();
    const int catalogChangesAfterCurrent = catalogSpy.count();
    const QString statusAfterCurrent = manager.statusText();
    const int statusChangesAfterCurrent = statusSpy.count();

    const auto persistedAfterCurrent = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterCurrent.isLoaded());
    const auto currentRecord = std::find_if(
        persistedAfterCurrent.snapshot.sources.begin(),
        persistedAfterCurrent.snapshot.sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(currentRecord != persistedAfterCurrent.snapshot.sources.end());
    const QByteArray persistedLineRows = currentRecord->constellationLineRows;
    QVERIFY(!persistedLineRows.isEmpty());

    QVERIFY(!staleReply.isNull());
    staleReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The previous incarnation's delayed response cannot replace the current
    // owner's dataset, its counts, its status, or the persisted collection.
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(manager.catalogRevision(), revisionAfterCurrent);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterCurrent);
    QCOMPARE(manager.statusText(), statusAfterCurrent);
    QCOMPARE(statusSpy.count(), statusChangesAfterCurrent);

    const auto persistedAfterStale = store.loadCatalogCollectionCache();
    QVERIFY(persistedAfterStale.isLoaded());
    const auto staleRecord = std::find_if(
        persistedAfterStale.snapshot.sources.begin(),
        persistedAfterStale.snapshot.sources.end(),
        [&sourceId](const SkySettingsStore::CatalogSourceCacheRecord& record) { return record.instanceId == sourceId; }
    );
    QVERIFY(staleRecord != persistedAfterStale.snapshot.sources.end());
    QCOMPARE(staleRecord->constellationLineRows, persistedLineRows);
}

void SkyCatalogManagerTests::disablingOwnerSupersedesItsPendingRelatedResponse()
{
    const QString catalogUrl = QStringLiteral("https://example.test/disabled-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/disabled-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));
    QVERIFY(manager.constellationLineRefs().empty());

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    manager.disableSource(sourceId);
    QVERIFY(!manager.isSourceEnabled(sourceId));

    const auto revisionAfterDisable = manager.catalogRevision();
    const int catalogChangesAfterDisable = catalogSpy.count();
    const int datasetChangesAfterDisable = datasetSpy.count();
    const QString statusAfterDisable = manager.statusText();
    const int statusChangesAfterDisable = statusSpy.count();

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // Disabling supersedes the owner's in-flight related work: the response is
    // discarded and cannot activate references or change counts or status.
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterDisable);
    QCOMPARE(catalogSpy.count(), catalogChangesAfterDisable);
    QCOMPARE(datasetSpy.count(), datasetChangesAfterDisable);
    QCOMPARE(manager.statusText(), statusAfterDisable);
    QCOMPARE(statusSpy.count(), statusChangesAfterDisable);

    // Re-enabling restores only data that completed before the disable.
    manager.enableSource(sourceId);
    QVERIFY(manager.isSourceEnabled(sourceId));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
}

void SkyCatalogManagerTests::lateRelatedFailureStatusFromSupersededOwnerIsIgnored()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-owner-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-owner-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.error = QNetworkReply::HostNotFoundError,
         .errorText = QStringLiteral("No fake response registered"),
         .httpStatusCode = 404,
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString sourceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    manager.disableSource(sourceId);
    const QString statusAfterDisable = manager.statusText();
    const int statusChangesAfterDisable = statusSpy.count();

    QVERIFY(!relatedReply.isNull());
    relatedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The failure status callback of the superseded request reports nothing.
    QCOMPARE(manager.statusText(), statusAfterDisable);
    QCOMPARE(statusSpy.count(), statusChangesAfterDisable);
}

void SkyCatalogManagerTests::outOfOrderRelatedRepliesPopulateTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/out-of-order-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/out-of-order-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/out-of-order-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/out-of-order-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> firstReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!firstReply.isNull());
    QVERIFY(!firstReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Source B's later request completes first; loading it did not discard
    // source A's pending response.
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});

    // Source A's earlier request completes after, into A's own record.
    QVERIFY(!firstReply.isNull());
    firstReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationCount(), std::size_t{2});

    // Each dataset stays with the owner whose response delivered it.
    manager.disableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    manager.enableSource(sourceAId);
    manager.disableSource(sourceBId);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerTests::reloadingOrRemovingAnotherSourceKeepsPendingOwnerResponse()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/pending-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/pending-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/pending-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/pending-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(sourceARelatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        findReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // Reloading and removing another source while the owner's response is
    // still pending must not supersede that response.
    manager.retrySource(sourceBId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.removeSource(sourceBId);
    QVERIFY(!manager.sourceInstanceIds().contains(sourceBId));
    QVERIFY(!pendingReply.isNull());
    QVERIFY(!pendingReply->isFinished());

    pendingReply->finishNow();
    QCoreApplication::processEvents();
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::reloadClearsOwnerRelatedDataBeforePublishing()
{
    const QString catalogUrl = QStringLiteral("https://example.test/reload-publish-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/reload-publish-lines.json");
    const QByteArray reloadedPayload =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 902001, .properName = "Reloaded Star", .mag = "2.0"});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = reloadedPayload});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = lyraRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // The reload is accepted while its replacement related reply is still
    // pending. The notification must already carry the cleared related state
    // that the runtime keeps from then on, not the dataset of the replaced
    // catalog, and that state must stay published until the next signal.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);

    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Reloaded Star")));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);

    QCoreApplication::processEvents();
    assertObservedStateMatchesRuntime(manager, observations.back());

    // Persisting during the pending interval keeps the accepted declaration
    // and the cleared dataset together, so a restart restores that same
    // intended state instead of the discarded references.
    {
        const auto pendingSnapshot = store.loadCatalogCollectionCache();
        QVERIFY(pendingSnapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record =
            findCollectionRecord(pendingSnapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(record->constellationLineRows.isEmpty());
        QVERIFY(record->constellationAnchorGroupRows.isEmpty());
        QCOMPARE(record->payload, reloadedPayload);
    }
    {
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
        QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Reloaded Star")));
    }

    // The accepted replacement publishes its own dataset in a further
    // notification that again matches the runtime state.
    skygate::ui::tests::FakeNetworkReply* replacementReply = findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(replacementReply != nullptr);
    replacementReply->finishNow();
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    assertObservedStateMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerTests::failedRelatedReplacementKeepsPublishedStateSynchronized()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.error = QNetworkReply::ContentNotFoundError,
         .errorText = QStringLiteral("Not Found"),
         .httpStatusCode = 404,
         .manualFinish = true}
    );
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = QByteArray("not a constellation payload\n"), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    // The replacement download fails with an HTTP error. The failure cannot
    // change the related view already published with the reload, so it emits
    // no further catalogChanged and the observer's view stays valid.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());

    const int changesBeforeHttpFailure = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* failedReply = findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(failedReply != nullptr);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/failed-replacement-lines\\.json .* HTTP 404")
    );
    failedReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeHttpFailure);
    QVERIFY(manager.constellationLineRefs().empty());
    assertObservedStateMatchesRuntime(manager, observations.back());

    // A replacement payload that cannot be parsed keeps the same cleared
    // state and likewise emits no further catalogChanged.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());

    const int changesBeforeParseFailure = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* unparsableReply = findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(unparsableReply != nullptr);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Constellation line parse failed"));
    unparsableReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeParseFailure);
    QVERIFY(manager.constellationLineRefs().empty());
    assertObservedStateMatchesRuntime(manager, observations.back());

    // Persistence carries the same cleared state.
    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerTests::canceledRelatedReplacementKeepsPublishedStateSynchronized()
{
    const QString catalogUrl = QStringLiteral("https://example.test/cancel-replacement-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/cancel-replacement-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = lyraRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());

    // Canceling supersedes the pending replacement without changing the
    // related view that was published with the accepted reload.
    const auto revisionBeforeCancel = manager.catalogRevision();
    const int changesBeforeCancel = catalogSpy.count();
    manager.cancelCatalogDownload();
    QCOMPARE(manager.statusText(), QStringLiteral("Catalog: Download canceled."));
    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QVERIFY(manager.constellationLineRefs().empty());
    assertObservedStateMatchesRuntime(manager, observations.back());

    // The canceled reply cannot attach anything afterwards.
    QTRY_VERIFY(allIssuedRepliesFinished(networkAccessManager));
    QCoreApplication::processEvents();
    QCOMPARE(manager.catalogRevision(), revisionBeforeCancel);
    QCOMPARE(catalogSpy.count(), changesBeforeCancel);
    QVERIFY(manager.constellationLineRefs().empty());
    assertObservedStateMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerTests::reloadClearsOnlyOwningRelatedDataset()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/scoped-reload-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/scoped-reload-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/scoped-reload-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/scoped-reload-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(
        sourceARelatedUrl,
        {.payload = relatedDatasetPayload(QStringLiteral("corona"), {27989, 25336, 25930}), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // Reloading source A clears only A's own dataset; the completed sibling
    // dataset and the published notification state stay untouched.
    manager.retrySource(sourceAId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(findPendingReplyForUrl(networkAccessManager, sourceARelatedUrl) != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    assertObservedStateMatchesRuntime(manager, observations.back());

    // A's replacement then joins the sibling's retained dataset.
    skygate::ui::tests::FakeNetworkReply* replacementReply =
        findPendingReplyForUrl(networkAccessManager, sourceARelatedUrl);
    QVERIFY(replacementReply != nullptr);
    replacementReply->finishNow();
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Corona") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(manager.constellationCount(), std::size_t{2});
    assertObservedStateMatchesRuntime(manager, observations.back());
}

void SkyCatalogManagerTests::relatedDatasetsRoundTripToTheirOwnSources()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/roundtrip-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/roundtrip-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/roundtrip-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/roundtrip-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordA = findCollectionRecord(snapshot.snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB = findCollectionRecord(snapshot.snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);

    // Each record carries the dataset its own owner downloaded; the composed
    // collection-wide view is not copied into both records.
    const auto recordALines = relatedLineRefs(*recordA);
    const auto recordBLines = relatedLineRefs(*recordB);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QCOMPARE(recordBLines.size(), std::size_t{2});
    QVERIFY(relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!relatedLineRefsContainHip(recordALines, "hip_26311"));
    QVERIFY(relatedLineRefsContainHip(recordBLines, "hip_26311"));
    QVERIFY(!relatedLineRefsContainHip(recordBLines, "hip_27989"));

    // The restart restores each dataset to its own owner.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QCOMPARE(restoredManager.constellationAnchorGroups().size(), std::size_t{2});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});

    restoredManager.disableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    restoredManager.enableSource(sourceAId);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
}

void SkyCatalogManagerTests::disabledOwnerRelatedDataStaysOwnedButInactiveAfterRestart()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/disabled-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/disabled-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/disabled-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/disabled-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    manager.disableSource(sourceAId);
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* recordA = findCollectionRecord(snapshot.snapshot, sourceAId);
    const SkySettingsStore::CatalogSourceCacheRecord* recordB = findCollectionRecord(snapshot.snapshot, sourceBId);
    QVERIFY(recordA != nullptr);
    QVERIFY(recordB != nullptr);
    QVERIFY(!recordA->enabled);

    // The disabled owner keeps its own inactive dataset; the persist does not
    // replace it with the composed view of the enabled sibling.
    const auto recordALines = relatedLineRefs(*recordA);
    QCOMPARE(recordALines.size(), std::size_t{2});
    QVERIFY(relatedLineRefsContainHip(recordALines, "hip_27989"));
    QVERIFY(!relatedLineRefsContainHip(recordALines, "hip_26311"));
    QCOMPARE(recordA->constellationCount, std::size_t{1});

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(!restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));

    // The disabled owner's references stay inactive after the restart.
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    // Re-enabling restores the owner's own dataset, not the sibling's.
    restoredManager.enableSource(sourceAId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    const skygate::ephemeris::ConstellationAnchorGroup* orionGroup =
        findConstellationAnchorGroup(restoredManager, "Orion");
    QVERIFY(orionGroup != nullptr);
    QCOMPARE(orionGroup->second.size(), std::size_t{3});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{2});
}

void SkyCatalogManagerTests::removedOwnerRelatedDataDoesNotReturnAfterRestart()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/removed-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/removed-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/removed-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/removed-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // A first restart restores both owners from their own records.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);

    // Removing one owner removes its dataset with its record.
    restoredManager.removeSource(sourceBId);
    QVERIFY(!restoredManager.sourceInstanceIds().contains(sourceBId));
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") == nullptr);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});

    const auto afterRemoval = store.loadCatalogCollectionCache();
    QVERIFY(afterRemoval.isLoaded());
    QVERIFY(!collectionContainsInstanceId(afterRemoval.snapshot, sourceBId));
    const SkySettingsStore::CatalogSourceCacheRecord* survivingRecord =
        findCollectionRecord(afterRemoval.snapshot, sourceAId);
    QVERIFY(survivingRecord != nullptr);
    const auto survivingLines = relatedLineRefs(*survivingRecord);
    QVERIFY(!relatedLineRefsContainHip(survivingLines, "hip_26311"));

    // The retired pre-collection two-slot cache still holds related data on
    // disk, but a committed collection snapshot keeps that fallback unused.
    SkySettingsStore::CatalogCacheSnapshot legacy;
    legacy.sourceLabel = QStringLiteral("Legacy Related");
    legacy.catalogPayload = skygate::ui::tests::orionHygCsvPayload();
    legacy.constellationLineRows = "hip_26311|hip_26727\n";
    legacy.constellationAnchorGroupRows = "Lyra|hip_26311,hip_26727\n";
    legacy.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    legacy.constellationCount = 1U;
    QVERIFY(store.saveCatalogCache(legacy));

    // The removed owner's dataset does not return through another record or
    // through the legacy fallback.
    SkyCatalogManager secondRestart(&store);
    QVERIFY(secondRestart.restoreCatalogCache());
    QVERIFY(!secondRestart.sourceInstanceIds().contains(sourceBId));
    QVERIFY(secondRestart.sourceInstanceIds().contains(sourceAId));
    QCOMPARE(secondRestart.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(secondRestart, "Orion") != nullptr);
    QVERIFY(findConstellationAnchorGroup(secondRestart, "Lyra") == nullptr);
    QCOMPARE(secondRestart.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::corruptOwnerRelatedPayloadLeavesSiblingDatasetIntact()
{
    const QString sourceAUrl = QStringLiteral("https://example.test/corrupt-owner-a-stars.csv");
    const QString sourceARelatedUrl = QStringLiteral("https://example.test/corrupt-owner-a-lines.json");
    const QString sourceBUrl = QStringLiteral("https://example.test/corrupt-owner-b-stars.csv");
    const QString sourceBRelatedUrl = QStringLiteral("https://example.test/corrupt-owner-b-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(sourceAUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceARelatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(sourceBUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(sourceBRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance sourceA =
        relatedDatasetInstance(sourceAUrl, sourceARelatedUrl);
    const QString sourceAId = sourceA.instanceId;
    manager.loadSource(sourceA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});

    const skygate::ui::internal::SkyCatalogSourceInstance sourceB =
        relatedDatasetInstance(sourceBUrl, sourceBRelatedUrl);
    const QString sourceBId = sourceB.instanceId;
    manager.loadSource(sourceB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});

    // Corrupt one owner's stored related payload in place.
    auto corruptedSnapshot = store.loadCatalogCollectionCache();
    QVERIFY(corruptedSnapshot.isLoaded());
    const auto corruptedRecord = std::find_if(
        corruptedSnapshot.snapshot.sources.begin(),
        corruptedSnapshot.snapshot.sources.end(),
        [&sourceAId](const SkySettingsStore::CatalogSourceCacheRecord& record) {
            return record.instanceId == sourceAId;
        }
    );
    QVERIFY(corruptedRecord != corruptedSnapshot.snapshot.sources.end());
    corruptedRecord->constellationLineRows = "not a related line payload";
    QVERIFY(store.saveCatalogCollectionCache(corruptedSnapshot.snapshot));

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Saved related constellation dataset is unreadable"));
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());

    // The corrupted owner restores without related data and keeps its
    // configuration; the sibling's dataset is untouched.
    QVERIFY(restoredManager.isSourceEnabled(sourceAId));
    QVERIFY(restoredManager.isSourceEnabled(sourceBId));
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});

    restoredManager.disableSource(sourceBId);
    QVERIFY(restoredManager.constellationLineRefs().empty());
    restoredManager.enableSource(sourceBId);
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
}

void SkyCatalogManagerTests::migratesPriorSingleOwnerRelatedPayloadOnce()
{
    // A snapshot written before the per-source related format: the bundled
    // record and one star record, where the star record holds the only copy of
    // the then-collection-wide related dataset.
    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion - 1;

    SkySettingsStore::CatalogSourceCacheRecord bundled;
    bundled.instanceId = QStringLiteral("primary");
    bundled.title = QStringLiteral("Bundled");
    bundled.bundled = true;
    bundled.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bundled.enabled = true;
    bundled.order = 0;
    snapshot.sources.push_back(std::move(bundled));

    SkySettingsStore::CatalogSourceCacheRecord star;
    star.instanceId = QStringLiteral("custom:prior-owner");
    star.title = QStringLiteral("Prior Owner");
    star.urls = QStringList{QStringLiteral("https://example.test/prior-owner.csv")};
    star.relatedDatasetUrls = QStringList{QStringLiteral("https://example.test/prior-owner-lines.json")};
    star.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    star.enabled = true;
    star.order = 1;
    star.payload = skygate::ui::tests::orionHygCsvPayload();
    star.constellationLineRows = "hip_27989|hip_25336\nhip_25336|hip_25930\n";
    star.constellationAnchorGroupRows = "Orion|hip_27989,hip_25336\n";
    star.constellationLineSchemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kConstellationLineCacheSchemaVersion;
    star.constellationCount = 1U;
    snapshot.sources.push_back(std::move(star));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    // The sole related payload identifies its owner, so the migration adopts
    // it instead of discarding it.
    SkyCatalogManager manager(&store);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // The migration rewrite commits the record in the per-source format and
    // keeps the adopted dataset under its owner.
    const auto migrated = store.loadCatalogCollectionCache();
    QVERIFY(migrated.isLoaded());
    QCOMPARE(
        migrated.snapshot.schemaVersion,
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion
    );
    const SkySettingsStore::CatalogSourceCacheRecord* migratedRecord =
        findCollectionRecord(migrated.snapshot, QStringLiteral("custom:prior-owner"));
    QVERIFY(migratedRecord != nullptr);
    QCOMPARE(relatedLineRefs(*migratedRecord).size(), std::size_t{2});

    // The next start reads the migrated records without another migration and
    // with the owner's dataset intact.
    SkyCatalogManager restarted(&store);
    QVERIFY(restarted.restoreCatalogCache());
    QCOMPARE(restarted.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(restarted, "Orion") != nullptr);
    QCOMPARE(restarted.constellationCount(), std::size_t{1});
}

void SkyCatalogManagerTests::removedBundledSourceDoesNotReturnAfterRestart()
{
    const QString siblingPath = m_settings.filePath(QStringLiteral("removed-bundled-sibling.csv"));
    QVERIFY(writeFile(
        siblingPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 906020, .hip = 906020, .properName = "Sibling Star", .mag = "1.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    manager.addSourceUrl(QUrl::fromLocalFile(siblingPath).toString(), QStringLiteral("Star"));
    QTRY_VERIFY(!manager.downloadingCatalog());
    const QString siblingId = manager.sourceInstanceIds().last();

    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.sourceInstanceIds(), QStringList{siblingId});

    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    QCOMPARE(snapshot.snapshot.sources.size(), 1);
    QCOMPARE(snapshot.snapshot.sources[0].instanceId, siblingId);

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{siblingId});
    const auto composedIds = restoredManager.sourceIds();
    QVERIFY(std::none_of(composedIds.begin(), composedIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("primary");
    }));
    QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Sibling Star")));
}

void SkyCatalogManagerTests::presentationSummarizesEnabledCollectionParticipation()
{
    const QString starAPath = m_settings.filePath(QStringLiteral("presentation-star-a.csv"));
    const QString starBPath = m_settings.filePath(QStringLiteral("presentation-star-b.csv"));
    const QString deepSkyPath = m_settings.filePath(QStringLiteral("presentation-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900701, .hip = 900701, .properName = "Presentation Star A", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900702, .hip = 900702, .properName = "Presentation Star B", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        skygate::ui::tests::sampleOpenNgcCsvPayload(
            {.name = "NGC0702",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0702",
             .identifiers = "PGC 7002",
             .commonName = "Presentation Galaxy"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);

    // The bundled star catalog already supplies the bundled deep-sky objects,
    // so the enabled fallback contributes nothing and is not named.
    QVERIFY(manager.participationSummary().startsWith(QStringLiteral("Bundled")));
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    // Three mixed configured sources: two star sources and one deep-sky source.
    skygate::ui::internal::SkyCatalogSourceInstance starA =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starAPath).toString());
    starA.title = QStringLiteral("Alpha");
    skygate::ui::internal::SkyCatalogSourceInstance starB =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starBPath).toString());
    starB.title = QStringLiteral("Beta");
    skygate::ui::internal::SkyCatalogSourceInstance deepSky =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(deepSkyPath).toString());
    deepSky.title = QStringLiteral("Gamma");

    manager.loadSource(starA, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(starB, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    manager.loadSource(deepSky, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The summary names every enabled source in visible collection order, and
    // the status text pairs it with the active snapshot counts.
    QCOMPARE(manager.sourceCount(), std::size_t{4});
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: %1 (").arg(manager.participationSummary())));
    const QLocale locale = QLocale::system();
    QVERIFY(manager.statusText().contains(
        QStringLiteral("(%1 objects,").arg(locale.toString(static_cast<qulonglong>(manager.bodyCount())))
    ));

    // Disabling one source removes it from the summary and keeps its own state
    // row, so a configured but disabled source is never presented as active.
    const int statusChangesBeforeDisable = statusSpy.count();
    manager.disableSource(starB.instanceId);
    QVERIFY(statusSpy.count() > statusChangesBeforeDisable);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Gamma"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Beta")));

    const auto findViewEntry = [](const QVector<SkyCatalogManager::SourceViewEntry>& entries,
                                  const QString& instanceId) {
        return std::find_if(
            entries.begin(), entries.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
                return entry.instanceId == instanceId;
            }
        );
    };
    const QVector<SkyCatalogManager::SourceViewEntry> disabledView = manager.sourceViewEntries();
    QCOMPARE(disabledView.size(), 4);
    const auto disabledEntry = findViewEntry(disabledView, starB.instanceId);
    QVERIFY(disabledEntry != disabledView.end());
    QVERIFY(!disabledEntry->enabled);
    QCOMPARE(disabledEntry->statusText, QStringLiteral("Disabled"));
    const auto enabledEntry = findViewEntry(disabledView, starA.instanceId);
    QVERIFY(enabledEntry != disabledView.end());
    QVERIFY(enabledEntry->enabled);
    QCOMPARE(enabledEntry->statusText, QStringLiteral("Active"));
    const auto deepSkyEntry = findViewEntry(disabledView, deepSky.instanceId);
    QVERIFY(deepSkyEntry != disabledView.end());
    QCOMPARE(deepSkyEntry->title, QStringLiteral("Gamma"));
    QCOMPARE(deepSkyEntry->policy, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    // Re-enabling restores the source in its collection position.
    manager.enableSource(starB.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Gamma"));

    // Reordering the collection reorders the summary.
    manager.moveSource(deepSky.instanceId, 1);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Alpha + Beta"));

    // Removing a source drops its row, its title, and its objects.
    manager.removeSource(starA.instanceId);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Gamma + Beta"));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Presentation Star A")));
    QCOMPARE(manager.sourceViewEntries().size(), 3);
}

void SkyCatalogManagerTests::bundledFallbackPresentationFollowsParticipationAndRestart()
{
    const QString starsPath = m_settings.filePath(QStringLiteral("fallback-stars.csv"));
    QVERIFY(writeFile(
        starsPath,
        skygate::ui::tests::sampleHygCsvPayload(
            {.id = 900703, .hip = 900703, .properName = "Fallback Star", .mag = "1.0"}
        )
    ));

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    // The bundled star source supplies the bundled deep-sky objects, so the
    // enabled fallback is not presented as the active deep-sky source.
    QVERIFY(!manager.participationSummary().contains(QStringLiteral("Bundled Messier")));

    skygate::ui::internal::SkyCatalogSourceInstance stars =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(QUrl::fromLocalFile(starsPath).toString());
    stars.title = QStringLiteral("Stars");
    manager.loadSource(stars, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Without the bundled star source the explicit fallback supplies the
    // deep-sky identities and the summary names it through its provenance.
    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core + Bundled Messier"));
    QVERIFY(manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // Turning the fallback participation off drops its name and its objects.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.retrySource(stars.instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.participationSummary(), QStringLiteral("Stars + Bundled core"));
    QVERIFY(!manager.statusText().contains(QStringLiteral("Bundled Messier")));

    // A restart restores the configured collection, the source titles, and the
    // fallback participation of the restarted configuration.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds().size(), 1);
    QVERIFY(restoredManager.participationSummary().startsWith(QStringLiteral("Stars (saved)")));
    QVERIFY(restoredManager.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(restoredManager.statusText().startsWith(QStringLiteral("Catalog: Stars (saved)")));
}

void SkyCatalogManagerTests::interchangeabilityScenarioKeepsObjectsProvenanceAndOwnedRelatedData()
{
    const std::vector<InterchangeabilityScenarioSource> sources = interchangeabilityScenarioSources();
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    enqueueInterchangeabilityPayloads(networkAccessManager, sources);

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    loadInterchangeabilityScenario(manager, sources);

    const QString anonymousInstanceId = sources[0].instance.instanceId;
    const QString secondInstanceId = sources[1].instance.instanceId;
    const QString archiveInstanceId = sources[2].instance.instanceId;

    // Two instances of one descriptor keep independently allocated identities,
    // and the archive source contributes its explicitly selected member only.
    QCOMPARE(sources[0].instance.descriptorId, sources[1].instance.descriptorId);
    QVERIFY(anonymousInstanceId != secondInstanceId);
    QCOMPARE(
        manager.sourceInstanceIds(),
        QStringList({QStringLiteral("primary"), anonymousInstanceId, secondInstanceId, archiveInstanceId})
    );
    QCOMPARE(
        manager.participationSummary(), QStringLiteral("Bundled + Anonymous Source + Second Instance + Archive Member")
    );
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: %1 (").arg(manager.participationSummary())));

    const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    QCOMPARE(manager.bodyCount(), catalog->bodies().size());
    QCOMPARE(manager.sourceIds().size(), catalog->bodies().size());
    QCOMPARE(manager.contributorSourceIds().size(), catalog->bodies().size());

    // The anonymous record key carries its owning instance, so the parser
    // counter of one source can never address the object of another, and the
    // bare counter never resolves to either object.
    const QString anonymousBodyId = QStringLiteral("hyg_auto_1@%1").arg(anonymousInstanceId);
    QVERIFY(findBodyById(catalog, anonymousBodyId) != nullptr);
    QVERIFY(findBodyById(catalog, QStringLiteral("hyg_auto_1")) == nullptr);

    // The unselected sibling member of the archive supplied no object.
    QVERIFY(findBodyById(catalog, QStringLiteral("ngc_999")) == nullptr);

    // The overlapping authoritative identifier merges into one survivor whose
    // provenance records the later winner and both contributors.
    const std::optional<std::size_t> hip2Index = bodyIndexById(catalog, QStringLiteral("hip_70002"));
    QVERIFY(hip2Index.has_value());
    QCOMPARE(catalog->bodies()[*hip2Index]->visualMagnitude, 5.5);
    QCOMPARE(manager.sourceIds()[*hip2Index], archiveInstanceId);
    QCOMPARE(manager.contributorSourceIds()[*hip2Index], QStringList({archiveInstanceId, secondInstanceId}));

    // Each source keeps its own local record ids: the second instance's record
    // "1" is HIP 70001 while the archive member's record "3" is HIP 70002,
    // and the merged survivor retains the local record identity of both the
    // record that won it and the record it absorbed.
    const std::optional<std::size_t> hip1Index = bodyIndexById(catalog, QStringLiteral("hip_70001"));
    const std::optional<std::size_t> hip3Index = bodyIndexById(catalog, QStringLiteral("hip_70003"));
    QVERIFY(hip1Index.has_value());
    QVERIFY(hip3Index.has_value());
    QVERIFY(*hip1Index != *hip2Index);
    QCOMPARE(QString::fromStdString(catalog->bodies()[*hip1Index]->identity.sourceRecordId), QStringLiteral("1"));
    QVERIFY(hasExternalIdentifier(*catalog->bodies()[*hip1Index], QStringLiteral("hyg"), QStringLiteral("1")));
    QCOMPARE(QString::fromStdString(catalog->bodies()[*hip2Index]->identity.sourceRecordId), QStringLiteral("3"));
    QVERIFY(hasExternalIdentifier(*catalog->bodies()[*hip2Index], QStringLiteral("hyg"), QStringLiteral("2")));
    QVERIFY(hasExternalIdentifier(*catalog->bodies()[*hip2Index], QStringLiteral("hyg"), QStringLiteral("3")));
    QCOMPARE(QString::fromStdString(catalog->bodies()[*hip3Index]->identity.sourceRecordId), QStringLiteral("4"));
    QCOMPARE(manager.sourceIds()[*hip1Index], secondInstanceId);
    QCOMPARE(manager.contributorSourceIds()[*hip1Index], QStringList{secondInstanceId});
    QCOMPARE(manager.sourceIds()[*hip3Index], archiveInstanceId);
    QCOMPARE(manager.contributorSourceIds()[*hip3Index], QStringList{archiveInstanceId});

    // Each source owns its own related dataset; the active view composes them
    // in visible collection order, resolved against the surviving bodies.
    QCOMPARE(
        activeAnchorNames(manager),
        QStringList({QStringLiteral("Orion"), QStringLiteral("Lyra"), QStringLiteral("Cygnus")})
    );
    QCOMPARE(
        resolvedAnchorNames(manager),
        QStringList({QStringLiteral("Orion"), QStringLiteral("Lyra"), QStringLiteral("Cygnus")})
    );
    QCOMPARE(
        resolvedLineRefTexts(manager),
        QStringList(
            {QStringLiteral("hip_70001|hip_70002"),
             QStringLiteral("hip_70002|hip_70001"),
             QStringLiteral("hip_70002|hip_70003")}
        )
    );

    // Disabling one owner drops only its contributions, and its retained
    // dataset returns with the source.
    manager.disableSource(secondInstanceId);
    QVERIFY(!manager.isSourceEnabled(secondInstanceId));
    QCOMPARE(activeAnchorNames(manager), QStringList({QStringLiteral("Orion"), QStringLiteral("Cygnus")}));
    QVERIFY(findBodyById(manager.starCatalog(), QStringLiteral("hip_70001")) == nullptr);
    QVERIFY(findBodyById(manager.starCatalog(), QStringLiteral("hip_70002")) != nullptr);
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled + Anonymous Source + Archive Member"));
    manager.enableSource(secondInstanceId);
    QCOMPARE(
        activeAnchorNames(manager),
        QStringList({QStringLiteral("Orion"), QStringLiteral("Lyra"), QStringLiteral("Cygnus")})
    );
    QVERIFY(findBodyById(manager.starCatalog(), QStringLiteral("hip_70001")) != nullptr);

    // Reloading the anonymous instance replaces its catalog and its owned
    // related dataset without re-allocating the local record key.
    manager.retrySource(anonymousInstanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(findConstellationAnchorGroup(manager, "Corona") != nullptr);
    const std::optional<std::size_t> reloadedIndex = bodyIndexById(manager.starCatalog(), anonymousBodyId);
    QVERIFY(reloadedIndex.has_value());
    QCOMPARE(manager.starCatalog()->bodies()[*reloadedIndex]->visualMagnitude, 5.0);
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(
        activeAnchorNames(manager),
        QStringList({QStringLiteral("Corona"), QStringLiteral("Lyra"), QStringLiteral("Cygnus")})
    );
}

void SkyCatalogManagerTests::sharedAliasNamesStayDistinctThroughManagerWorkflow()
{
    const QString url = QStringLiteral("https://example.test/shared-alias.csv");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        url,
        {.payload = "Name;Type;RA;Dec;Common names\n"
                    "NGC0001;G;01:00:00;+02:00:00;Shared region\n"
                    "NGC0002;G;10:00:00;+20:00:00;Shared region\n"}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(url);
    source.title = QStringLiteral("Shared Alias Source");
    source.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;

    // A shared descriptive name never discards a recognized catalog
    // designation, even when the optional cross-reference columns are absent.
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept ngc_2 distinct from ngc_1 because their shared alias is ambiguous across "
        "authoritative identifiers."
    );
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    const std::optional<std::size_t> firstIndex = bodyIndexById(catalog, QStringLiteral("ngc_1"));
    const std::optional<std::size_t> secondIndex = bodyIndexById(catalog, QStringLiteral("ngc_2"));
    QVERIFY(firstIndex.has_value());
    QVERIFY(secondIndex.has_value());

    const skygate::ephemeris::BaseCelestialBody& first = *catalog->bodies()[*firstIndex];
    const skygate::ephemeris::BaseCelestialBody& second = *catalog->bodies()[*secondIndex];
    QCOMPARE(first.kind, skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject);
    QCOMPARE(second.kind, skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject);
    QVERIFY(hasExternalIdentifier(first, QStringLiteral("ngc"), QStringLiteral("1")));
    QVERIFY(hasExternalIdentifier(second, QStringLiteral("ngc"), QStringLiteral("2")));
    QVERIFY(hasAlias(first, QStringLiteral("Shared region")));
    QVERIFY(hasAlias(second, QStringLiteral("Shared region")));
    QCOMPARE(QString::fromStdString(first.displayName), QStringLiteral("NGC 1"));
    QCOMPARE(QString::fromStdString(second.displayName), QStringLiteral("NGC 2"));
    QVERIFY(first.fixedEquatorialValue().has_value());
    QVERIFY(second.fixedEquatorialValue().has_value());
    QCOMPARE(first.fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(second.fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(manager.sourceIds()[*firstIndex], source.instanceId);
    QCOMPARE(manager.sourceIds()[*secondIndex], source.instanceId);
    QCOMPARE(manager.contributorSourceIds()[*firstIndex], QStringList{source.instanceId});
    QCOMPARE(manager.contributorSourceIds()[*secondIndex], QStringList{source.instanceId});
}

void SkyCatalogManagerTests::conflictingAstrometryKeepsOneCoherentModelThroughManager()
{
    const QString astrometricUrl = QStringLiteral("https://example.test/astrometric.csv");
    const QString fixedUrl = QStringLiteral("https://example.test/fixed-only.csv");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        astrometricUrl, {.payload = "hip,ra,dec,mag,pmra,pmdec\n70001,1,2,3,125.0,-55.0\n"}
    );
    networkAccessManager.enqueueResponse(fixedUrl, {.payload = "hip,ra,dec,mag\n70001,10,20,4\n"});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance astrometric =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(astrometricUrl);
    astrometric.title = QStringLiteral("Astrometric Source");
    skygate::ui::internal::SkyCatalogSourceInstance fixedOnly =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(fixedUrl);
    fixedOnly.title = QStringLiteral("Fixed Source");

    // The later source wins the position and carries no astrometry, so the
    // contradicting astrometry of the earlier source is rejected instead of
    // being copied beside the winning fixed coordinates.
    manager.loadSource(astrometric, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            "Catalog composition kept the fixed coordinates of hip_70001 over conflicting fixed "
            "coordinates from hip_70001\\."
        ))
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            "Catalog composition kept the fixed coordinates of hip_70001 and rejected the "
            "incompatible astrometry of hip_70001\\."
        ))
    );
    manager.loadSource(fixedOnly, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    const std::optional<std::size_t> bodyIndex = bodyIndexById(catalog, QStringLiteral("hip_70001"));
    QVERIFY(bodyIndex.has_value());
    const skygate::ephemeris::BaseCelestialBody& survivor = *catalog->bodies()[*bodyIndex];
    QVERIFY(survivor.fixedEquatorialValue().has_value());
    QCOMPARE(survivor.fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(survivor.fixedEquatorialValue()->declinationDeg, 20.0);
    QCOMPARE(survivor.visualMagnitude, 4.0);
    QVERIFY(!survivor.starAstrometryValue().has_value());
    QCOMPARE(manager.sourceIds()[*bodyIndex], fixedOnly.instanceId);
    QCOMPARE(manager.contributorSourceIds()[*bodyIndex], QStringList({fixedOnly.instanceId, astrometric.instanceId}));
}

void SkyCatalogManagerTests::bundledDeepSkyFallbackFillsGapsWithoutOverridingConfiguredChoice()
{
    const QString configuredUrl = QStringLiteral("https://example.test/configured-m31.csv");
    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        configuredUrl,
        {.payload = "Name;Type;RA;Dec;M;NGC;V-Mag;Identifiers;Common names\n"
                    "NGC0224;G;00:42:44.35;+41:16:08.6;31;0224;0.0;PGC 2557;Andromeda Galaxy\n"}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    skygate::ui::internal::SkyCatalogSourceInstance configured =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(configuredUrl);
    configured.title = QStringLiteral("Configured M31");
    configured.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv;
    manager.loadSource(configured, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // Without the bundled star source the bundled deep-sky fallback joins
    // the composition, and it fills only the identities no configured source
    // supplies instead of overriding them.
    manager.removeSource(QStringLiteral("primary"));
    QCOMPARE(manager.sourceInstanceIds(), QStringList{configured.instanceId});
    QCOMPARE(manager.participationSummary(), QStringLiteral("Configured M31 + Bundled core + Bundled Messier"));

    const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    const std::optional<std::size_t> m31Index = bodyIndexById(catalog, QStringLiteral("messier_031"));
    QVERIFY(m31Index.has_value());
    QCOMPARE(catalog->bodies()[*m31Index]->visualMagnitude, 0.0);
    QCOMPARE(manager.sourceIds()[*m31Index], configured.instanceId);
    QCOMPARE(manager.contributorSourceIds()[*m31Index], QStringList{configured.instanceId});

    const std::optional<std::size_t> m1Index = bodyIndexById(catalog, QStringLiteral("messier_001"));
    QVERIFY(m1Index.has_value());
    QCOMPARE(manager.sourceIds()[*m1Index], QStringLiteral("bundled-deep-sky"));
    QCOMPARE(manager.sourceTitle(QStringLiteral("bundled-deep-sky")), QStringLiteral("Bundled Messier"));

    // The fallback participation and the configured value survive a restart.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.sourceInstanceIds(), QStringList{configured.instanceId});
    QCOMPARE(
        restoredManager.participationSummary(),
        QStringLiteral("Configured M31 (saved) + Bundled core + Bundled Messier")
    );
    const skygate::ephemeris::IStarCatalog* restoredCatalog = restoredManager.starCatalog();
    QVERIFY(restoredCatalog != nullptr);
    const std::optional<std::size_t> restoredM31Index = bodyIndexById(restoredCatalog, QStringLiteral("messier_031"));
    QVERIFY(restoredM31Index.has_value());
    QCOMPARE(restoredCatalog->bodies()[*restoredM31Index]->visualMagnitude, 0.0);
    QCOMPARE(restoredManager.sourceIds()[*restoredM31Index], configured.instanceId);
}

void SkyCatalogManagerTests::crossSourceHipBridgesSurviveBinaryCollectionRestore()
{
    const auto makeHipHdStar = [](std::string id,
                                  std::string displayName,
                                  const std::string& hip,
                                  const std::string& hd,
                                  const std::string& hyg) {
        skygate::ephemeris::OwnGalaxyCelestialBody body;
        body.id = std::move(id);
        body.displayName = std::move(displayName);
        body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
        body.visualMagnitude = 1.0;
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
        if (!hip.empty()) {
            body.identity.externalIdentifiers.push_back(skygate::ephemeris::CatalogIdentifier::make("hip", hip));
        }
        if (!hd.empty()) {
            body.identity.externalIdentifiers.push_back(skygate::ephemeris::CatalogIdentifier::make("hd", hd));
        }
        if (!hyg.empty()) {
            body.identity.externalIdentifiers.push_back(skygate::ephemeris::CatalogIdentifier::make("hyg", hyg));
        }
        return body;
    };

    // One source establishes that HIP 1 and HD 2 are the same object, and
    // the later source supplies both identifiers through two records. The
    // identifiers arrive only through the versioned binary cache, because no
    // shipped schema emits an HD designation.
    auto establishingCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeHipHdStar("a_hip1_hd2", "Established", "1", "2", {})}
    );
    auto bridgingCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeHipHdStar("b_hip1", {}, "1", {}, {}), makeHipHdStar("b_hd2", "Later record", {}, "2", "5")}
    );
    QVERIFY(establishingCatalog != nullptr);
    QVERIFY(bridgingCatalog != nullptr);

    SkySettingsStore::CatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkySettingsStore::CatalogSourceCacheRecord establishing;
    establishing.instanceId = QStringLiteral("bridge-a");
    establishing.title = QStringLiteral("Establishing Source");
    establishing.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    establishing.enabled = true;
    establishing.order = 0;
    establishing.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(establishingCatalog->catalog());
    QVERIFY(!establishing.binaryPayload.isEmpty());
    snapshot.sources.push_back(std::move(establishing));

    SkySettingsStore::CatalogSourceCacheRecord bridging;
    bridging.instanceId = QStringLiteral("bridge-b");
    bridging.title = QStringLiteral("Bridging Source");
    bridging.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    bridging.enabled = true;
    bridging.order = 1;
    bridging.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(bridgingCatalog->catalog());
    QVERIFY(!bridging.binaryPayload.isEmpty());
    snapshot.sources.push_back(std::move(bridging));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(snapshot));

    SkyCatalogManager manager(&store);
    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("bridge-a"), QStringLiteral("bridge-b")}));

    const skygate::ephemeris::IStarCatalog* catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    QVERIFY(findBodyById(catalog, QStringLiteral("a_hip1_hd2")) == nullptr);
    QVERIFY(findBodyById(catalog, QStringLiteral("b_hd2")) == nullptr);
    const std::optional<std::size_t> bridgeIndex = bodyIndexById(catalog, QStringLiteral("b_hip1"));
    QVERIFY(bridgeIndex.has_value());
    const skygate::ephemeris::BaseCelestialBody& survivor = *catalog->bodies()[*bridgeIndex];
    QVERIFY(hasExternalIdentifier(survivor, QStringLiteral("hip"), QStringLiteral("1")));
    QVERIFY(hasExternalIdentifier(survivor, QStringLiteral("hd"), QStringLiteral("2")));
    QVERIFY(hasExternalIdentifier(survivor, QStringLiteral("hyg"), QStringLiteral("5")));
    QCOMPARE(QString::fromStdString(survivor.displayName), QStringLiteral("Established"));
    QVERIFY(survivor.fixedEquatorialValue().has_value());
    QCOMPARE(survivor.fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(manager.sourceIds()[*bridgeIndex], QStringLiteral("bridge-b"));
    QCOMPARE(
        manager.contributorSourceIds()[*bridgeIndex],
        QStringList({QStringLiteral("bridge-b"), QStringLiteral("bridge-a")})
    );

    // The restored collection is the intended snapshot, and restoring it again
    // from the same binary records reproduces it exactly.
    const CollectionSnapshotFingerprint expected = collectionFingerprint(manager);
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    compareCollectionFingerprints(expected, collectionFingerprint(restoredManager));
}

void SkyCatalogManagerTests::collectionLifecycleKeepsSnapshotAndIdentitiesAcrossRestart()
{
    std::vector<InterchangeabilityScenarioSource> configured = interchangeabilityScenarioSources();
    configured.push_back(interchangeabilityRemovableSource());

    const QString anonymousInstanceId = configured[0].instance.instanceId;
    const QString secondInstanceId = configured[1].instance.instanceId;
    const QString archiveInstanceId = configured[2].instance.instanceId;
    const QString removableInstanceId = configured[3].instance.instanceId;

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    enqueueInterchangeabilityPayloads(networkAccessManager, configured);

    const QString cacheDirectory = m_settings.filePath(QStringLiteral("lifecycle-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    CollectionSnapshotFingerprint expected;
    {
        SkySettingsStore store;
        SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
        loadInterchangeabilityScenario(manager, configured);
        QCOMPARE(
            manager.sourceInstanceIds(),
            QStringList(
                {QStringLiteral("primary"),
                 anonymousInstanceId,
                 secondInstanceId,
                 archiveInstanceId,
                 removableInstanceId}
            )
        );

        // The overlapping HIP survivor records the later archive member as its
        // winner and both instances as contributors before any mutation.
        const std::optional<std::size_t> mergedHipIndex =
            bodyIndexById(manager.starCatalog(), QStringLiteral("hip_70002"));
        QVERIFY(mergedHipIndex.has_value());
        QCOMPARE(manager.sourceIds()[*mergedHipIndex], archiveInstanceId);
        QCOMPARE(manager.contributorSourceIds()[*mergedHipIndex], QStringList({archiveInstanceId, secondInstanceId}));

        // Reorder the collection: the archive member moves before the source
        // whose identifier it overlaps.
        manager.moveSource(archiveInstanceId, 1);
        QCOMPARE(
            manager.sourceInstanceIds(),
            QStringList(
                {QStringLiteral("primary"),
                 archiveInstanceId,
                 anonymousInstanceId,
                 secondInstanceId,
                 removableInstanceId}
            )
        );

        // Disabling a source removes its contributions without discarding its
        // owned related dataset, and the overlapping survivor falls back to the
        // earlier instance that supplies the same identifier.
        manager.disableSource(archiveInstanceId);
        QVERIFY(!manager.isSourceEnabled(archiveInstanceId));
        QVERIFY(findBodyById(manager.starCatalog(), QStringLiteral("hip_70003")) == nullptr);
        const std::optional<std::size_t> fallenBackIndex =
            bodyIndexById(manager.starCatalog(), QStringLiteral("hip_70002"));
        QVERIFY(fallenBackIndex.has_value());
        QCOMPARE(manager.sourceIds()[*fallenBackIndex], secondInstanceId);
        QCOMPARE(manager.contributorSourceIds()[*fallenBackIndex], QStringList{secondInstanceId});
        QCOMPARE(manager.starCatalog()->bodies()[*fallenBackIndex]->visualMagnitude, 5.0);

        // Reloading one instance replaces its catalog and related dataset
        // without re-allocating its durable instance identity.
        manager.retrySource(anonymousInstanceId);
        QTRY_VERIFY(!manager.downloadingCatalog());
        QTRY_VERIFY(findConstellationAnchorGroup(manager, "Corona") != nullptr);
        QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
        const QString anonymousBodyId = QStringLiteral("hyg_auto_1@%1").arg(anonymousInstanceId);
        const std::optional<std::size_t> anonymousIndex = bodyIndexById(manager.starCatalog(), anonymousBodyId);
        QVERIFY(anonymousIndex.has_value());
        QCOMPARE(manager.starCatalog()->bodies()[*anonymousIndex]->visualMagnitude, 5.0);

        // Removing the disposable source drops its object and its instance.
        manager.removeSource(removableInstanceId);
        QCOMPARE(
            manager.sourceInstanceIds(),
            QStringList({QStringLiteral("primary"), archiveInstanceId, anonymousInstanceId, secondInstanceId})
        );
        QVERIFY(findBodyById(manager.starCatalog(), QStringLiteral("hip_70005")) == nullptr);

        // The reloaded anonymous record keeps its qualified key next to the
        // survivors of the collection.
        QCOMPARE(activeAnchorNames(manager), QStringList({QStringLiteral("Corona"), QStringLiteral("Lyra")}));

        expected = collectionFingerprint(manager);
        QCOMPARE(expected.bodyCount, manager.starCatalog()->bodies().size());
    }

    // Restart 1: the versioned binary cache restores the same collection. The
    // anonymous source's raw payload is removed first, so a source that could
    // only be restored by reparsing it would lose its catalog.
    const QString anonymousRawPath =
        catalogSourceSidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".txt"));
    QVERIFY(QFile::remove(anonymousRawPath));
    QVERIFY(!QFile::exists(anonymousRawPath));
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".bin"))));
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin"))));
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin"))));
    {
        SkySettingsStore store;
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        compareCollectionFingerprints(expected, collectionFingerprint(restoredManager));

        // The disabled owner's related dataset was restored with its source and
        // stays inactive until the source is enabled again.
        QVERIFY(findConstellationAnchorGroup(restoredManager, "Cygnus") == nullptr);
        restoredManager.enableSource(archiveInstanceId);
        QCOMPARE(restoredManager.constellationAnchorGroups().size(), std::size_t{3});
        QVERIFY(findConstellationAnchorGroup(restoredManager, "Cygnus") != nullptr);
        restoredManager.disableSource(archiveInstanceId);
        QVERIFY(findConstellationAnchorGroup(restoredManager, "Cygnus") == nullptr);
    }

    // Restart 2: without a binary sidecar every source is restored by
    // reparsing its stored raw payload, including the selected archive member.
    QVERIFY(QFile::remove(catalogSourceSidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin"))));
    QVERIFY(QFile::remove(catalogSourceSidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin"))));

    // Restoring the reloaded raw payload is only possible if the fallback has
    // the same bytes the reload produced.
    QFile anonymousRawFile(anonymousRawPath);
    QVERIFY(anonymousRawFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(
        anonymousRawFile.write(configured[0].reloadedCatalogPayload),
        qint64(configured[0].reloadedCatalogPayload.size())
    );
    anonymousRawFile.close();
    {
        SkySettingsStore store;
        SkyCatalogManager fallbackManager(&store);
        QVERIFY(fallbackManager.restoreCatalogCache());
        compareCollectionFingerprints(expected, collectionFingerprint(fallbackManager));
    }

    // The raw-payload fallback upgraded the restored records back into the
    // versioned binary cache.
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".bin"))));
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin"))));
    QVERIFY(QFile::exists(catalogSourceSidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin"))));
}

void SkyCatalogManagerTests::legacySourcesStayRetiredAfterMigratedCollectionIsEmptied()
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
        QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
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
        QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
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
    QVERIFY(!catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("Legacy Star")));
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
    QVERIFY(!catalogContainsDisplayName(emptyCollectionManager.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy Custom")));
    QVERIFY(!emptyCollectionManager.sourceTitles().values().contains(QStringLiteral("Legacy OpenNGC")));
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerTests::unreadableCommittedCollectionKeepsActiveCollectionAndReportsFailure()
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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));

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
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Accepted Star")));
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Legacy Star")));
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
    QVERIFY(!catalogContainsDisplayName(restarted.starCatalog(), QStringLiteral("Legacy Star")));
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
    QVERIFY(store.loadCatalogCache().has_value());
}

void SkyCatalogManagerTests::missingConfigurationKeepsFirstUseDefaults()
{
    // This scenario needs a settings state with no stored configuration at all,
    // so its cache paths are isolated from the shared per-suite cache files.
    m_settings.resetForCurrentTest();

    SkySettingsStore store;
    SkyCatalogManager manager(&store);

    // First use has no stored configuration, so the bundled default source is
    // configured and composes the initial snapshot.
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(manager.isSourceEnabled(QStringLiteral("primary")));
    QVERIFY(manager.starCatalog() != nullptr);
    QVERIFY(manager.bodyCount() > 0U);
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());

    // Nothing was ever stored: restore reports that and keeps the first-use
    // default instead of clearing the collection.
    QVERIFY(!manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());

    // A fresh manager over the still empty configuration receives the same
    // first-use default.
    SkyCatalogManager restarted(&store);
    QVERIFY(!restarted.restoreCatalogCache());
    QCOMPARE(restarted.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(restarted.isSourceEnabled(QStringLiteral("primary")));
    QVERIFY(restarted.starCatalog() != nullptr);
    QVERIFY(!store.loadCatalogCollectionCache().isLoaded());
}

void SkyCatalogManagerTests::removingSoleSourceLeavesEmptyCollection()
{
    SkySettingsStore store;
    SkyCatalogManager manager(&store);
    QCOMPARE(manager.sourceCount(), std::size_t{1});
    QSignalSpy sourcesSpy(&manager, &SkyCatalogManager::sourcesChanged);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);
    QSignalSpy datasetSpy(&manager, &SkyCatalogManager::datasetInfoTextChanged);

    manager.removeSource(QStringLiteral("primary"));

    // The removed instance is neither recreated nor replaced by an implicit
    // preset row, and the removal is published to the UI.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceInstanceIds().isEmpty());
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QVERIFY(!manager.sourceTitles().contains(QStringLiteral("primary")));
    QVERIFY(catalogSpy.count() >= 1);
    QVERIFY(datasetSpy.count() >= 1);
    QVERIFY(sourcesSpy.count() >= 1);

    // The active snapshot is the documented bundled augmentation: the bundled
    // core and the bundled deep-sky fallback participate under their own
    // provenance without a configured source row.
    QVERIFY(manager.starCatalog() != nullptr);
    QVERIFY(manager.bodyCount() > 0U);
    const auto bodies = manager.starCatalog()->bodies();
    const std::size_t deepSkyBodyCount = static_cast<std::size_t>(
        std::count_if(bodies.begin(), bodies.end(), [](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
        })
    );
    QVERIFY(deepSkyBodyCount > 0U);
    QCOMPARE(manager.sourceLabel(), QStringLiteral("Bundled core"));
    QCOMPARE(manager.participationSummary(), QStringLiteral("Bundled core + Bundled Messier"));
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Bundled core + Bundled Messier")));
    const std::span<const QString> composedSourceIds = manager.sourceIds();
    QVERIFY(!composedSourceIds.empty());
    QVERIFY(std::all_of(composedSourceIds.begin(), composedSourceIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("bundled-core") || sourceId == QStringLiteral("bundled-deep-sky");
    }));

    // Removing the last source commits the empty collection instead of
    // clearing the configuration back to first use.
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerTests::savedEmptyCollectionRestoresEmptyAcrossRestarts()
{
    SkySettingsStore store;
    {
        SkyCatalogManager manager(&store);
        QCOMPARE(manager.sourceCount(), std::size_t{1});
        manager.removeSource(QStringLiteral("primary"));
        QCOMPARE(manager.sourceCount(), std::size_t{0});

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // Restart 1: the stored empty snapshot restores successfully and stays
    // empty; the first-use default is not reinstated over it.
    {
        SkyCatalogManager firstRestart(&store);
        QSignalSpy sourcesSpy(&firstRestart, &SkyCatalogManager::sourcesChanged);
        QSignalSpy catalogSpy(&firstRestart, &SkyCatalogManager::catalogChanged);
        QVERIFY(firstRestart.restoreCatalogCache());
        QCOMPARE(firstRestart.sourceCount(), std::size_t{0});
        QVERIFY(firstRestart.sourceInstanceIds().isEmpty());
        QVERIFY(firstRestart.sourceViewEntries().isEmpty());
        QCOMPARE(firstRestart.participationSummary(), QStringLiteral("Bundled core + Bundled Messier"));
        QVERIFY(catalogSpy.count() >= 1);
        QVERIFY(sourcesSpy.count() >= 1);

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }

    // Restart 2: the configuration is still empty and no default source has
    // crept back into the stored snapshot.
    {
        SkyCatalogManager secondRestart(&store);
        QVERIFY(secondRestart.restoreCatalogCache());
        QCOMPARE(secondRestart.sourceCount(), std::size_t{0});
        QVERIFY(secondRestart.sourceInstanceIds().isEmpty());
        QVERIFY(secondRestart.sourceViewEntries().isEmpty());

        const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
        QVERIFY(persisted.isLoaded());
        QVERIFY(persisted.snapshot.sources.isEmpty());
    }
}

void SkyCatalogManagerTests::updateRemovingRelatedDeclarationRetiresOwnedData()
{
    const QString catalogUrl = QStringLiteral("https://example.test/removed-declaration-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/removed-declaration-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/removed-declaration-update.csv");
    const QByteArray updatePayload =
        skygate::ui::tests::sampleHygCsvPayload({.hip = 902101, .properName = "No Related Update Star", .mag = "2.0"});

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // The accepted update declares no related dataset anymore. The obsolete
    // owned data retires in the same accepted transition, before publication.
    skygate::ui::internal::SkyCatalogSourceInstance updated = source;
    updated.title = QStringLiteral("No Related Update");
    updated.version = QStringLiteral("v2");
    updated.urls = QStringList{updateUrl};
    updated.relatedDatasetUrls.clear();
    manager.loadSource(updated, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("No Related Update Star")));
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());

    // The persisted snapshot carries the accepted declaration and retired
    // rows together, so a restart cannot resurrect the retired dataset.
    {
        const auto snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QVERIFY(record->relatedDatasetUrls.isEmpty());
        QVERIFY(record->constellationLineRows.isEmpty());
        QVERIFY(record->constellationAnchorGroupRows.isEmpty());
        QCOMPARE(record->payload, updatePayload);
    }
    {
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
        QVERIFY(catalogContainsDisplayName(restoredManager.starCatalog(), QStringLiteral("No Related Update Star")));

        // Disabling and re-enabling cannot resurrect retired owned data: no
        // related declaration exists to request it from.
        networkAccessManager.enqueueResponse(updateUrl, {.payload = updatePayload});
        restoredManager.disableSource(instanceId);
        restoredManager.enableSource(instanceId);
        QCoreApplication::processEvents();
        QVERIFY(restoredManager.constellationLineRefs().empty());
        QVERIFY(restoredManager.constellationAnchorGroups().empty());
        QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
    }
}

void SkyCatalogManagerTests::failedUpdateAttemptingToRemoveRelatedDeclarationKeepsDataset()
{
    const QString catalogUrl = QStringLiteral("https://example.test/failed-removal-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/failed-removal-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/failed-removal-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.error = QNetworkReply::ContentNotFoundError, .errorText = QStringLiteral("Not Found"), .httpStatusCode = 404}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    const auto starCountBefore = manager.bodyCount();

    // The update that would have removed the declaration is rejected, so the
    // accepted source and its owned dataset stay coherent and published.
    skygate::ui::internal::SkyCatalogSourceInstance rejected = source;
    rejected.title = QStringLiteral("Rejected Removal");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    rejected.relatedDatasetUrls.clear();
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.bodyCount(), starCountBefore);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    assertObservedStateMatchesRuntime(manager, observations.back());

    // Persistence still serializes the accepted declaration and rows.
    manager.moveSource(instanceId, 0);
    const auto snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
}

void SkyCatalogManagerTests::lateReplyToRemovedRelatedDeclarationIsRejected()
{
    const QString catalogUrl = QStringLiteral("https://example.test/late-removal-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/late-removal-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/late-removal-update.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload({.hip = 902102, .properName = "Late Update Star"})}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(findPendingReplyForUrl(networkAccessManager, relatedUrl) != nullptr);

    // The update retires the declaration while the old related reply is still
    // pending. The accepted transition must not keep the old dataset active.
    skygate::ui::internal::SkyCatalogSourceInstance updated = source;
    updated.version = QStringLiteral("v2");
    updated.urls = QStringList{updateUrl};
    updated.relatedDatasetUrls.clear();
    manager.loadSource(updated, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());

    // The late reply belongs to the superseded revision and cannot attach
    // data or change the published related view.
    const int changesBeforeLateReply = catalogSpy.count();
    skygate::ui::tests::FakeNetworkReply* lateReply = findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(lateReply != nullptr);
    lateReply->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeLateReply);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Late Update Star")));
}

void SkyCatalogManagerTests::bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy_data()
{
    QTest::addColumn<int>("policy");
    QTest::addRow("merge") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTest::addRow("deep-sky-only") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QTest::addRow("deep-sky-fallback") << static_cast<int>(
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyFallback
    );
    QTest::addRow("augment-core") << static_cast<int>(skygate::ephemeris::CatalogCompositionPolicy::AugmentCore);
}

void SkyCatalogManagerTests::bundledUpdateRetiresOwnedRelatedDataUnderEveryPolicy()
{
    QFETCH(int, policy);
    const auto bundledPolicy = static_cast<skygate::ephemeris::CatalogCompositionPolicy>(policy);

    const QString catalogUrl = QStringLiteral("https://example.test/bundled-policy-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/bundled-policy-lines.json");
    const QString siblingUrl = QStringLiteral("https://example.test/bundled-policy-sibling-stars.csv");
    const QString siblingRelatedUrl = QStringLiteral("https://example.test/bundled-policy-sibling-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(siblingUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(siblingRelatedUrl, {.payload = lyraRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // A second owner keeps its own dataset throughout, so the retirement stays
    // scoped to the instance whose accepted declaration changed.
    const skygate::ui::internal::SkyCatalogSourceInstance sibling =
        relatedDatasetInstance(siblingUrl, siblingRelatedUrl);
    manager.loadSource(sibling, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{4});
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);

    // The update keeps the durable instance ID but activates bundled content
    // whose declaration selects no related dataset.
    skygate::ui::internal::SkyCatalogSourceInstance update = source;
    update.title = QStringLiteral("Bundled Policy Update");
    update.version = QStringLiteral("v2");
    update.urls.clear();
    update.relatedDatasetUrls.clear();
    manager.loadSource(update, bundledPolicy);

    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") != nullptr);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{1});
    QCOMPARE(observations.back().constellationCount, std::size_t{1});
    assertObservedStateMatchesRuntime(manager, observations.back());

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto updatedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(updatedEntry != view.end());
    QVERIFY(updatedEntry->bundled);
    QVERIFY(!updatedEntry->hasError);

    // The accepted declaration and the retired dataset are persisted together,
    // and the sibling record keeps its own payload.
    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* updatedRecord =
        findCollectionRecord(snapshot.snapshot, instanceId);
    const SkySettingsStore::CatalogSourceCacheRecord* siblingRecord =
        findCollectionRecord(snapshot.snapshot, sibling.instanceId);
    QVERIFY(updatedRecord != nullptr);
    QVERIFY(siblingRecord != nullptr);
    QVERIFY(updatedRecord->relatedDatasetUrls.isEmpty());
    QVERIFY(updatedRecord->constellationLineRows.isEmpty());
    QVERIFY(updatedRecord->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(updatedRecord->bundled, true);
    QCOMPARE(siblingRecord->relatedDatasetUrls, QStringList{siblingRelatedUrl});
    QVERIFY(!siblingRecord->constellationLineRows.isEmpty());
    QVERIFY(!siblingRecord->constellationAnchorGroupRows.isEmpty());

    // A restart agrees with the live state instead of resurrecting the retired
    // dataset.
    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QCOMPARE(restoredManager.constellationLineRefs().size(), std::size_t{2});
    QCOMPARE(restoredManager.constellationCount(), std::size_t{1});
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Orion") == nullptr);
    QVERIFY(findConstellationAnchorGroup(restoredManager, "Lyra") != nullptr);
}

void SkyCatalogManagerTests::payloadOriginSwitchesFollowTheAcceptedRelatedDeclaration()
{
    const QString catalogUrl = QStringLiteral("https://example.test/origin-switch-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/origin-switch-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    // The instance starts as bundled content and owns no related dataset.
    skygate::ui::internal::SkyCatalogSourceInstance source =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    source.title = QStringLiteral("Origin Switch Source");
    source.urls.clear();
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());

    // A downloaded update of the same instance declares its own related
    // dataset, which becomes owned once its own reply completes.
    skygate::ui::internal::SkyCatalogSourceInstance downloaded = source;
    downloaded.version = QStringLiteral("v2");
    downloaded.urls = QStringList{catalogUrl};
    downloaded.relatedDatasetUrls = QStringList{relatedUrl};
    manager.loadSource(downloaded, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    // Switching the same instance back to bundled content retires the dataset
    // exactly like a downloaded update that removes the declaration.
    skygate::ui::internal::SkyCatalogSourceInstance bundled = downloaded;
    bundled.version = QStringLiteral("v3");
    bundled.urls.clear();
    bundled.relatedDatasetUrls.clear();
    manager.loadSource(bundled, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());

    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->relatedDatasetUrls.isEmpty());
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());

    SkyCatalogManager restoredManager(&store);
    QVERIFY(restoredManager.restoreCatalogCache());
    QVERIFY(restoredManager.constellationLineRefs().empty());
    QVERIFY(restoredManager.constellationAnchorGroups().empty());
    QCOMPARE(restoredManager.constellationCount(), std::size_t{0});
}

void SkyCatalogManagerTests::rejectedBundledUpdateKeepsOwnedRelatedData()
{
    const QString catalogUrl = QStringLiteral("https://example.test/rejected-bundled-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/rejected-bundled-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);

    // The bundled deep-sky fallback is disabled while the source loads, so its
    // reserved identity is available to the configured source. Re-enabling the
    // fallback later makes any transition of that source collide with the
    // implicit entry.
    manager.setDeepSkyCatalogPresetIndex(1);

    skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    source.instanceId = QStringLiteral("bundled-deep-sky");
    source.title = QStringLiteral("Rejected Bundled Source");
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);

    manager.setDeepSkyCatalogPresetIndex(0);

    // The bundled update cannot compose against the enabled fallback, so the
    // replacement is rejected and the accepted dataset stays published.
    skygate::ui::internal::SkyCatalogSourceInstance update = source;
    update.title = QStringLiteral("Rejected Bundled Update");
    update.version = QStringLiteral("v2");
    update.urls.clear();
    update.relatedDatasetUrls.clear();
    manager.loadSource(update, skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly);

    const QVector<SkyCatalogManager::SourceViewEntry> view = manager.sourceViewEntries();
    const auto updatedEntry =
        std::find_if(view.begin(), view.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(updatedEntry != view.end());
    QVERIFY(updatedEntry->hasError);
    QVERIFY(updatedEntry->statusText.contains(QStringLiteral("rejected")));
    QCOMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QCOMPARE(manager.constellationCount(), std::size_t{1});
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    assertObservedStateMatchesRuntime(manager, observations.back());

    // The rejected attempt leaves the accepted declaration and its dataset in
    // the persisted record.
    manager.setDeepSkyCatalogPresetIndex(1);
    manager.moveSource(instanceId, 0);
    const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
    QVERIFY(snapshot.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(!record->constellationLineRows.isEmpty());
    QVERIFY(!record->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(record->bundled, false);
}

void SkyCatalogManagerTests::combinedAcceptedFactsSurviveRelatedReplacementRejectionReorderAndRawRestart()
{
    const QString cacheDirectory = m_settings.filePath(QStringLiteral("combined-accepted-facts-cache"));
    QDir(cacheDirectory).removeRecursively();
    QSettings settings;
    settings.setValue(QStringLiteral("skyContext/catalogCollectionCachePath"), cacheDirectory);

    const QString acceptedUrl = QStringLiteral("https://example.test/combined-accepted-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/combined-accepted-lines.json");
    const QString updateUrl = QStringLiteral("https://example.test/combined-update-stars.csv");
    const QString siblingUrl = QStringLiteral("https://example.test/combined-sibling-stars.csv");
    const QByteArray acceptedPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 903001, .hip = 903001, .properName = "Combined Accepted Star", .mag = "2.0"}
    );
    const QByteArray siblingPayload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 903002, .hip = 903002, .properName = "Combined Sibling Star", .mag = "3.0"}
    );

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(siblingUrl, {.payload = siblingPayload});
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = lyraRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationState> observations;
    observeCatalogNotifications(manager, observations);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    // Step 1: the owner loads with a resolvable related dataset. Its published
    // and persisted state agree.
    skygate::ui::internal::SkyCatalogSourceInstance accepted = relatedDatasetInstance(acceptedUrl, relatedUrl);
    accepted.title = QStringLiteral("Combined Accepted Title");
    accepted.version = QStringLiteral("v1");
    const QString instanceId = accepted.instanceId;
    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") != nullptr);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{1});
    QCOMPARE(observations.back().constellationCount, std::size_t{1});
    assertObservedStateMatchesRuntime(manager, observations.back());
    {
        const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->payload, skygate::ui::tests::orionHygCsvPayload());
        QVERIFY(relatedLineRefsContainHip(relatedLineRefs(*record), "hip_27989"));
    }

    // Step 2: a second configurable source, so the later reorder has two rows.
    skygate::ui::internal::SkyCatalogSourceInstance sibling =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(siblingUrl, QStringLiteral("v1"));
    sibling.title = QStringLiteral("Combined Sibling Title");
    const QString siblingId = sibling.instanceId;
    manager.loadSource(sibling, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Combined Sibling Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId, siblingId}));
    assertObservedStateMatchesRuntime(manager, observations.back());

    // Step 3: the accepted source reloads while its replacement related reply
    // stays pending. The cleared dataset the runtime keeps is the state every
    // notification reports and the state that gets persisted.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReplacement =
        findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!pendingReplacement.isNull());
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Combined Accepted Star")));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    assertObservedStateMatchesRuntime(manager, observations.back());
    QCoreApplication::processEvents();
    assertObservedStateMatchesRuntime(manager, observations.back());
    {
        const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->payload, acceptedPayload);
        QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(record->constellationLineRows.isEmpty());
        QVERIFY(record->constellationAnchorGroupRows.isEmpty());
    }

    // Step 4: a descriptor update for the same instance is rejected with an
    // HTTP error. It installs nothing, supersedes the still-pending
    // replacement, and leaves the published state, the revision, and the
    // stored accepted facts untouched.
    const int changesBeforeRejectedUpdate = catalogSpy.count();
    const std::uint64_t revisionBeforeRejectedUpdate = manager.catalogRevision();
    networkAccessManager.enqueueResponse(
        updateUrl,
        {.error = QNetworkReply::ContentNotFoundError, .errorText = QStringLiteral("Not Found"), .httpStatusCode = 404}
    );
    skygate::ui::internal::SkyCatalogSourceInstance rejected = accepted;
    rejected.title = QStringLiteral("Combined Rejected Title");
    rejected.version = QStringLiteral("v2");
    rejected.urls = QStringList{updateUrl};
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Catalog source failed https://example\\.test/combined-update-stars\\.csv .* HTTP 404")
    );
    manager.loadSource(rejected, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    const QVector<SkyCatalogManager::SourceViewEntry> failedView = manager.sourceViewEntries();
    const auto failedEntry = std::find_if(
        failedView.begin(), failedView.end(), [&instanceId](const SkyCatalogManager::SourceViewEntry& entry) {
            return entry.instanceId == instanceId;
        }
    );
    QVERIFY(failedEntry != failedView.end());
    QVERIFY(failedEntry->hasError);
    QVERIFY(failedEntry->statusText.contains(QStringLiteral("HTTP 404")));
    QCOMPARE(catalogSpy.count(), changesBeforeRejectedUpdate);
    QCOMPARE(manager.catalogRevision(), revisionBeforeRejectedUpdate);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Combined Accepted Star")));
    assertObservedStateMatchesRuntime(manager, observations.back());

    // The superseded replacement cannot publish its dataset after the failure.
    QVERIFY(!pendingReplacement.isNull());
    pendingReplacement->finishNow();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(catalogSpy.count(), changesBeforeRejectedUpdate);
    QCOMPARE(manager.catalogRevision(), revisionBeforeRejectedUpdate);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.constellationCount(), std::size_t{0});
    QVERIFY(findConstellationAnchorGroup(manager, "Lyra") == nullptr);
    assertObservedStateMatchesRuntime(manager, observations.back());
    {
        const SkySettingsStore::CatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->title, QStringLiteral("Combined Accepted Title"));
        QCOMPARE(record->version, QStringLiteral("v1"));
        QCOMPARE(record->urls, QStringList{acceptedUrl});
        QCOMPARE(record->payload, acceptedPayload);
        QCOMPARE(record->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(record->constellationLineRows.isEmpty());
    }

    // Step 5: reordering saves the collection in its new configured order while
    // every stored fact stays the accepted one.
    manager.moveSource(siblingId, 0);
    QCOMPARE(manager.sourceInstanceIds(), QStringList({siblingId, QStringLiteral("primary"), instanceId}));
    assertObservedStateMatchesRuntime(manager, observations.back());
    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QStringList persistedInstanceIds;
    for (const SkySettingsStore::CatalogSourceCacheRecord& record : persisted.snapshot.sources) {
        persistedInstanceIds.push_back(record.instanceId);
    }
    QCOMPARE(persistedInstanceIds, manager.sourceInstanceIds());
    const SkySettingsStore::CatalogSourceCacheRecord* persistedRecord =
        findCollectionRecord(persisted.snapshot, instanceId);
    QVERIFY(persistedRecord != nullptr);
    QCOMPARE(persistedRecord->title, QStringLiteral("Combined Accepted Title"));
    QCOMPARE(persistedRecord->version, QStringLiteral("v1"));
    QCOMPARE(persistedRecord->urls, QStringList{acceptedUrl});
    QCOMPARE(persistedRecord->payload, acceptedPayload);
    QCOMPARE(persistedRecord->relatedDatasetUrls, QStringList{relatedUrl});
    QVERIFY(persistedRecord->constellationLineRows.isEmpty());
    QVERIFY(persistedRecord->constellationAnchorGroupRows.isEmpty());

    // Step 6: without the binary sidecars the restart reparses the accepted raw
    // payloads and rebuilds exactly the stored configuration, including the
    // declared but still empty related dataset.
    const QString acceptedBinaryPath = catalogSourceSidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    const QString siblingBinaryPath = catalogSourceSidecarPath(cacheDirectory, siblingId, QStringLiteral(".bin"));
    QVERIFY(!acceptedBinaryPath.isEmpty());
    QVERIFY(!siblingBinaryPath.isEmpty());
    QVERIFY(QFile::remove(acceptedBinaryPath));
    QVERIFY(QFile::remove(siblingBinaryPath));

    SkyCatalogManager rawRestart(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(rawRestart.restoreCatalogCache());
    QCOMPARE(rawRestart.sourceInstanceIds(), persistedInstanceIds);
    QVERIFY(catalogContainsDisplayName(rawRestart.starCatalog(), QStringLiteral("Combined Accepted Star")));
    QVERIFY(catalogContainsDisplayName(rawRestart.starCatalog(), QStringLiteral("Combined Sibling Star")));
    QCOMPARE(durableTitle(rawRestart.sourceTitles().value(instanceId)), QStringLiteral("Combined Accepted Title"));
    QCOMPARE(durableTitle(rawRestart.sourceTitles().value(siblingId)), QStringLiteral("Combined Sibling Title"));
    QVERIFY(rawRestart.constellationLineRefs().empty());
    QVERIFY(rawRestart.constellationAnchorGroups().empty());
    QCOMPARE(rawRestart.constellationCount(), std::size_t{0});
    {
        const SkySettingsStore::CatalogCollectionCacheLoadResult upgraded = store.loadCatalogCollectionCache();
        QVERIFY(upgraded.isLoaded());
        const SkySettingsStore::CatalogSourceCacheRecord* upgradedRecord =
            findCollectionRecord(upgraded.snapshot, instanceId);
        QVERIFY(upgradedRecord != nullptr);
        QCOMPARE(upgradedRecord->title, QStringLiteral("Combined Accepted Title"));
        QCOMPARE(upgradedRecord->version, QStringLiteral("v1"));
        QCOMPARE(upgradedRecord->urls, QStringList{acceptedUrl});
        QCOMPARE(upgradedRecord->payload, acceptedPayload);
        QCOMPARE(upgradedRecord->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(upgradedRecord->constellationLineRows.isEmpty());
    }
}

void SkyCatalogManagerTests::liveRestoreDropsOperationsOmittedFromTheRestoredCollection()
{
    const QString catalogUrl = QStringLiteral("https://example.test/live-restore-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        catalogUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910200, .hip = 910200, .properName = "Live Restore Star", .mag = "1.0"}
         )}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QTRY_VERIFY(!manager.downloadingCatalog());
    QCOMPARE(manager.sourceCount(), std::size_t{2});
    QCOMPARE(manager.sourceViewEntries().size(), 2);
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Live Restore Star")));

    // The committed configuration is now empty even though this manager still
    // holds the source it loaded, as after a settings restore that replaces
    // the session's collection.
    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    QVERIFY(manager.restoreCatalogCache());

    // The accepted restore replaces the runtime collection and the operation
    // rows together: an omitted source stays neither active nor visible as a
    // leftover pending row.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceInstanceIds().isEmpty());
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Live Restore Star")));

    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerTests::liveRestoreSupersedesPendingImportReply()
{
    const QString pendingUrl = QStringLiteral("https://example.test/live-restore-pending-stars.csv");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        pendingUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910001, .hip = 910001, .properName = "Superseded Pending Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    manager.loadSource(
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(pendingUrl),
        skygate::ephemeris::CatalogCompositionPolicy::Merge
    );
    QVERIFY(manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        findPendingReplyForUrl(networkAccessManager, pendingUrl);
    QVERIFY(!pendingReply.isNull());

    SkySettingsStore::CatalogCollectionCacheSnapshot emptyCollection;
    emptyCollection.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    emptyCollection.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);
    QVERIFY(store.saveCatalogCollectionCache(emptyCollection));

    QVERIFY(manager.restoreCatalogCache());
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(!manager.catalogProcessing());
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceViewEntries().isEmpty());
    const std::uint64_t revisionAfterRestore = manager.catalogRevision();

    pendingReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The reply of the superseded import cannot repopulate the accepted
    // collection, create a row, or publish a catalog change.
    QCOMPARE(manager.sourceCount(), std::size_t{0});
    QVERIFY(manager.sourceViewEntries().isEmpty());
    QCOMPARE(manager.catalogRevision(), revisionAfterRestore);
    QVERIFY(!catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Superseded Pending Star")));

    const SkySettingsStore::CatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QVERIFY(persisted.snapshot.sources.isEmpty());
}

void SkyCatalogManagerTests::liveRestoreSupersedesPendingRelatedReplyForReusedInstanceId()
{
    const QString catalogUrl = QStringLiteral("https://example.test/live-restore-reused-stars.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/live-restore-reused-lines.json");

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(catalogUrl, {.payload = skygate::ui::tests::orionHygCsvPayload()});
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = orionRelatedDatasetPayload(), .manualFinish = true});

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);

    const skygate::ui::internal::SkyCatalogSourceInstance source = relatedDatasetInstance(catalogUrl, relatedUrl);
    const QString instanceId = source.instanceId;
    manager.loadSource(source, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_VERIFY(networkAccessManager.requestedUrls().contains(relatedUrl));

    QPointer<skygate::ui::tests::FakeNetworkReply> relatedReply = findReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!relatedReply.isNull());
    QVERIFY(!relatedReply->isFinished());

    // The stored configuration names the same instance the live manager is
    // already waiting on a related reply for, so the restored source reuses
    // the pending instance's ID.
    const SkySettingsStore::CatalogCollectionCacheLoadResult beforeRestore = store.loadCatalogCollectionCache();
    QVERIFY(beforeRestore.isLoaded());
    QVERIFY(collectionContainsInstanceId(beforeRestore.snapshot, instanceId));

    QVERIFY(manager.restoreCatalogCache());
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId}));
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    const std::uint64_t revisionAfterRestore = manager.catalogRevision();
    const QString statusAfterRestore = manager.statusText();

    relatedReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The previous incarnation's pending related reply cannot attach its
    // dataset to the restored instance that reused the ID, and it leaves the
    // restored counts, status, and persisted record untouched.
    QVERIFY(findConstellationAnchorGroup(manager, "Orion") == nullptr);
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QCOMPARE(manager.catalogRevision(), revisionAfterRestore);
    QCOMPARE(manager.statusText(), statusAfterRestore);

    const SkySettingsStore::CatalogCollectionCacheLoadResult afterStale = store.loadCatalogCollectionCache();
    QVERIFY(afterStale.isLoaded());
    const SkySettingsStore::CatalogSourceCacheRecord* record = findCollectionRecord(afterStale.snapshot, instanceId);
    QVERIFY(record != nullptr);
    QVERIFY(record->constellationLineRows.isEmpty());
    QVERIFY(record->constellationAnchorGroupRows.isEmpty());
    QCOMPARE(record->constellationCount, std::size_t{0});
}

void SkyCatalogManagerTests::rejectedRestoreKeepsPendingImportWork()
{
    const QString pendingUrl = QStringLiteral("https://example.test/rejected-restore-pending-stars.csv");

    // A persisted instance identity colliding with the implicit bundled core
    // source makes the restored collection uncomposable, so the restore is
    // rejected before it can replace the live collection.
    SkySettingsStore::CatalogCollectionCacheSnapshot rejectedSnapshot;
    rejectedSnapshot.schemaVersion =
        skygate::ui::internal::SkyContextControllerConstants::kCatalogCollectionCacheSchemaVersion;
    SkySettingsStore::CatalogSourceCacheRecord colliding;
    colliding.instanceId = QStringLiteral("bundled-core");
    colliding.title = QStringLiteral("Colliding Source");
    colliding.urls = QStringList{QStringLiteral("https://example.test/colliding-stars.csv")};
    colliding.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    colliding.enabled = true;
    colliding.order = 0;
    colliding.payload = skygate::ui::tests::sampleHygCsvPayload(
        {.id = 907001, .hip = 907001, .properName = "Colliding Star", .mag = "1.0"}
    );
    rejectedSnapshot.sources.push_back(std::move(colliding));

    SkySettingsStore store;
    QVERIFY(store.saveCatalogCollectionCache(rejectedSnapshot));

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(
        pendingUrl,
        {.payload = skygate::ui::tests::sampleHygCsvPayload(
             {.id = 910101, .hip = 910101, .properName = "Still Valid Pending Star", .mag = "1.0"}
         ),
         .manualFinish = true}
    );

    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    QSignalSpy statusSpy(&manager, &SkyCatalogManager::statusTextChanged);
    const skygate::ui::internal::SkyCatalogSourceInstance pending =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(pendingUrl);
    const QString instanceId = pending.instanceId;
    manager.loadSource(pending, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReply =
        findPendingReplyForUrl(networkAccessManager, pendingUrl);
    QVERIFY(!pendingReply.isNull());

    const QStringList sourcesBefore = manager.sourceInstanceIds();
    const QVector<SkyCatalogManager::SourceViewEntry> rowsBefore = manager.sourceViewEntries();
    QCOMPARE(rowsBefore.size(), 2);
    const int statusChangesAfterStart = statusSpy.count();

    QVERIFY(!manager.restoreCatalogCache());
    QVERIFY(manager.statusText().startsWith(QStringLiteral("Catalog: Collection rejected")));
    QCOMPARE(statusSpy.count(), statusChangesAfterStart + 1);

    // The rejected restore keeps the previous collection, its rows, and the
    // still-valid import instead of discarding the pending work.
    QCOMPARE(manager.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(manager.sourceViewEntries().size(), rowsBefore.size());
    QVERIFY(manager.downloadingCatalog());
    const QVector<SkyCatalogManager::SourceViewEntry> rowsAfterRejection = manager.sourceViewEntries();
    const auto pendingRow =
        std::find_if(rowsAfterRejection.begin(), rowsAfterRejection.end(), [&instanceId](const auto& entry) {
            return entry.instanceId == instanceId;
        });
    QVERIFY(pendingRow != rowsAfterRejection.end());
    QVERIFY(pendingRow->busy);

    pendingReply->finishNow();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // The preserved import still completes into the unchanged collection.
    QVERIFY(!manager.downloadingCatalog());
    QVERIFY(manager.sourceInstanceIds().contains(instanceId));
    QVERIFY(catalogContainsDisplayName(manager.starCatalog(), QStringLiteral("Still Valid Pending Star")));
}

QTEST_GUILESS_MAIN(SkyCatalogManagerTests)

#include "SkyCatalogManagerTests.moc"
