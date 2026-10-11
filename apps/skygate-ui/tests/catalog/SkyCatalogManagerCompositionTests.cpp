#include "BaseCelestialBody.hpp"
#include "CatalogArchiveTestSupport.hpp"
#include "CatalogBodyQuery.hpp"
#include "CatalogNotificationRecorder.hpp"
#include "CatalogSourceFactsQuery.hpp"
#include "CatalogTestPayloads.hpp"
#include "ConstellationAnchorGroupLookup.hpp"
#include "ConstellationTestSupport.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "FakeReplyQuery.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "RelatedDatasetFixture.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyCatalogPresets.hpp"
#include "SkyCatalogSourceDescriptor.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyContextControllerSupport.hpp"
#include "SkySettingsStore.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogCollectionStore.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using skygate::ui::tests::CatalogBodyQuery;
using skygate::ui::tests::CatalogNotificationRecorder;
using skygate::ui::tests::CatalogSourceFactsQuery;
using skygate::ui::tests::ConstellationAnchorGroupLookup;
using skygate::ui::tests::FakeReplyQuery;
using skygate::ui::tests::RelatedDatasetFixture;

namespace {

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
            .relatedPayload = RelatedDatasetFixture::relatedDatasetPayload(QStringLiteral("orion"), {70001, 70002}),
            .reloadedCatalogPayload = "ra,dec,mag\n3,4,5\n",
            .reloadedRelatedPayload =
                RelatedDatasetFixture::relatedDatasetPayload(QStringLiteral("corona"), {70001, 70002}),
        },
        InterchangeabilityScenarioSource{
            .instance = std::move(second),
            .catalogPayload = "id,hip,ra,dec,mag\n1,70001,5,5,4.0\n2,70002,6,6,5.0\n",
            .relatedPayload = RelatedDatasetFixture::relatedDatasetPayload(QStringLiteral("lyra"), {70002, 70001}),
        },
        InterchangeabilityScenarioSource{
            .instance = std::move(archive),
            .catalogPayload = scenarioArchiveZip(),
            .relatedPayload = RelatedDatasetFixture::relatedDatasetPayload(QStringLiteral("cygnus"), {70002, 70003}),
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
        fingerprint.titles.push_back(CatalogSourceFactsQuery::durableTitle(titles.value(instanceId)));
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

}  // namespace

class SkyCatalogManagerCompositionTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void interchangeabilityKeepsProvenanceAndRelatedData();
    void sharedAliasNamesStayDistinctThroughManager();
    void conflictingAstrometryKeepsOneCoherentModel();
    void bundledFallbackFillsGapsKeepingConfiguredChoice();
    void crossSourceHipBridgesSurviveBinaryRestore();
    void collectionKeepsSnapshotAndIdentitiesOnRestart();
    void combinedFactsSurviveRejectionReorderRawRestart();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyCatalogManagerCompositionTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyCatalogManagerCompositionTests")));
}

void SkyCatalogManagerCompositionTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
    // Clearing the settings does not remove the cache directory, and a
    // manifest left there would describe a committed collection whose records
    // the cleared settings no longer hold. Every test starts from the state
    // its scenario describes: no committed collection.
    QDir(m_settings.filePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void SkyCatalogManagerCompositionTests::interchangeabilityKeepsProvenanceAndRelatedData()
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
    QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Corona") != nullptr);
    const std::optional<std::size_t> reloadedIndex = bodyIndexById(manager.starCatalog(), anonymousBodyId);
    QVERIFY(reloadedIndex.has_value());
    QCOMPARE(manager.starCatalog()->bodies()[*reloadedIndex]->visualMagnitude, 5.0);
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QCOMPARE(
        activeAnchorNames(manager),
        QStringList({QStringLiteral("Corona"), QStringLiteral("Lyra"), QStringLiteral("Cygnus")})
    );
}

void SkyCatalogManagerCompositionTests::sharedAliasNamesStayDistinctThroughManager()
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

void SkyCatalogManagerCompositionTests::conflictingAstrometryKeepsOneCoherentModel()
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

void SkyCatalogManagerCompositionTests::bundledFallbackFillsGapsKeepingConfiguredChoice()
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

void SkyCatalogManagerCompositionTests::crossSourceHipBridgesSurviveBinaryRestore()
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

    SkyCatalogCollectionCacheSnapshot snapshot;
    snapshot.schemaVersion = skygate::ui::internal::SkyCatalogCollectionStore::kCollectionSchemaVersion;
    snapshot.binarySchemaVersion = static_cast<int>(skygate::ephemeris::CatalogBinaryCodec::kSchemaVersion);

    SkyCatalogSourceCacheRecord establishing;
    establishing.instanceId = QStringLiteral("bridge-a");
    establishing.title = QStringLiteral("Establishing Source");
    establishing.policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
    establishing.enabled = true;
    establishing.order = 0;
    establishing.binaryPayload = skygate::ephemeris::CatalogBinaryCodec::serialize(establishingCatalog->catalog());
    QVERIFY(!establishing.binaryPayload.isEmpty());
    snapshot.sources.push_back(std::move(establishing));

    SkyCatalogSourceCacheRecord bridging;
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

void SkyCatalogManagerCompositionTests::collectionKeepsSnapshotAndIdentitiesOnRestart()
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
        QTRY_VERIFY(ConstellationAnchorGroupLookup::find(manager, "Corona") != nullptr);
        QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
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
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".txt"));
    QVERIFY(QFile::remove(anonymousRawPath));
    QVERIFY(!QFile::exists(anonymousRawPath));
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".bin")))
    );
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin")))
    );
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin")))
    );
    {
        SkySettingsStore store;
        SkyCatalogManager restoredManager(&store);
        QVERIFY(restoredManager.restoreCatalogCache());
        compareCollectionFingerprints(expected, collectionFingerprint(restoredManager));

        // The disabled owner's related dataset was restored with its source and
        // stays inactive until the source is enabled again.
        QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Cygnus") == nullptr);
        restoredManager.enableSource(archiveInstanceId);
        QCOMPARE(restoredManager.constellationAnchorGroups().size(), std::size_t{3});
        QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Cygnus") != nullptr);
        restoredManager.disableSource(archiveInstanceId);
        QVERIFY(ConstellationAnchorGroupLookup::find(restoredManager, "Cygnus") == nullptr);
    }

    // Restart 2: without a binary sidecar every source is restored by
    // reparsing its stored raw payload, including the selected archive member.
    QVERIFY(
        QFile::remove(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin")))
    );
    QVERIFY(
        QFile::remove(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin")))
    );

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
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, anonymousInstanceId, QStringLiteral(".bin")))
    );
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, secondInstanceId, QStringLiteral(".bin")))
    );
    QVERIFY(
        QFile::exists(CatalogSourceFactsQuery::sidecarPath(cacheDirectory, archiveInstanceId, QStringLiteral(".bin")))
    );
}

void SkyCatalogManagerCompositionTests::combinedFactsSurviveRejectionReorderRawRestart()
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
    networkAccessManager.enqueueResponse(relatedUrl, {.payload = RelatedDatasetFixture::orionRelatedDatasetPayload()});
    networkAccessManager.enqueueResponse(siblingUrl, {.payload = siblingPayload});
    networkAccessManager.enqueueResponse(acceptedUrl, {.payload = acceptedPayload});
    networkAccessManager.enqueueResponse(
        relatedUrl, {.payload = RelatedDatasetFixture::lyraRelatedDatasetPayload(), .manualFinish = true}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    std::vector<CatalogNotificationRecorder::State> observations;
    CatalogNotificationRecorder::observe(manager, observations);
    QSignalSpy catalogSpy(&manager, &SkyCatalogManager::catalogChanged);

    // Step 1: the owner loads with a resolvable related dataset. Its published
    // and persisted state agree.
    skygate::ui::internal::SkyCatalogSourceInstance accepted =
        RelatedDatasetFixture::relatedDatasetInstance(acceptedUrl, relatedUrl);
    accepted.title = QStringLiteral("Combined Accepted Title");
    accepted.version = QStringLiteral("v1");
    const QString instanceId = accepted.instanceId;
    manager.loadSource(accepted, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationLineRefs().size(), std::size_t{2});
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") != nullptr);
    QVERIFY(!observations.empty());
    QCOMPARE(observations.back().lineRefCount, std::size_t{2});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{1});
    QCOMPARE(observations.back().constellationCount, std::size_t{1});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    {
        const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
        QVERIFY(record != nullptr);
        QCOMPARE(record->payload, skygate::ui::tests::orionHygCsvPayload());
        QVERIFY(
            CatalogSourceFactsQuery::relatedLineRefsContainHip(
                CatalogSourceFactsQuery::relatedLineRefs(*record), "hip_27989"
            )
        );
    }

    // Step 2: a second configurable source, so the later reorder has two rows.
    skygate::ui::internal::SkyCatalogSourceInstance sibling =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(siblingUrl, QStringLiteral("v1"));
    sibling.title = QStringLiteral("Combined Sibling Title");
    const QString siblingId = sibling.instanceId;
    manager.loadSource(sibling, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Combined Sibling Star")));
    QCOMPARE(manager.sourceInstanceIds(), QStringList({QStringLiteral("primary"), instanceId, siblingId}));
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

    // Step 3: the accepted source reloads while its replacement related reply
    // stays pending. The cleared dataset the runtime keeps is the state every
    // notification reports and the state that gets persisted.
    manager.retrySource(instanceId);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QPointer<skygate::ui::tests::FakeNetworkReply> pendingReplacement =
        FakeReplyQuery::findPendingReplyForUrl(networkAccessManager, relatedUrl);
    QVERIFY(!pendingReplacement.isNull());
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Combined Accepted Star")));
    QVERIFY(manager.constellationLineRefs().empty());
    QVERIFY(manager.constellationAnchorGroups().empty());
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Orion") == nullptr);
    QCOMPARE(observations.back().lineRefCount, std::size_t{0});
    QCOMPARE(observations.back().anchorGroupCount, std::size_t{0});
    QCOMPARE(observations.back().constellationCount, std::size_t{0});
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    QCoreApplication::processEvents();
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    {
        const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
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
    QVERIFY(CatalogBodyQuery::containsDisplayName(manager.starCatalog(), QStringLiteral("Combined Accepted Star")));
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());

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
    QVERIFY(ConstellationAnchorGroupLookup::find(manager, "Lyra") == nullptr);
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    {
        const SkyCatalogCollectionCacheLoadResult snapshot = store.loadCatalogCollectionCache();
        QVERIFY(snapshot.isLoaded());
        const SkyCatalogSourceCacheRecord* record = CatalogSourceFactsQuery::findRecord(snapshot.snapshot, instanceId);
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
    CatalogNotificationRecorder::assertMatchesRuntime(manager, observations.back());
    const SkyCatalogCollectionCacheLoadResult persisted = store.loadCatalogCollectionCache();
    QVERIFY(persisted.isLoaded());
    QStringList persistedInstanceIds;
    for (const SkyCatalogSourceCacheRecord& record : persisted.snapshot.sources) {
        persistedInstanceIds.push_back(record.instanceId);
    }
    QCOMPARE(persistedInstanceIds, manager.sourceInstanceIds());
    const SkyCatalogSourceCacheRecord* persistedRecord =
        CatalogSourceFactsQuery::findRecord(persisted.snapshot, instanceId);
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
    const QString acceptedBinaryPath =
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, instanceId, QStringLiteral(".bin"));
    const QString siblingBinaryPath =
        CatalogSourceFactsQuery::sidecarPath(cacheDirectory, siblingId, QStringLiteral(".bin"));
    QVERIFY(!acceptedBinaryPath.isEmpty());
    QVERIFY(!siblingBinaryPath.isEmpty());
    QVERIFY(QFile::remove(acceptedBinaryPath));
    QVERIFY(QFile::remove(siblingBinaryPath));

    SkyCatalogManager rawRestart(&store, nullptr, nullptr, &networkAccessManager);
    QVERIFY(rawRestart.restoreCatalogCache());
    QCOMPARE(rawRestart.sourceInstanceIds(), persistedInstanceIds);
    QVERIFY(CatalogBodyQuery::containsDisplayName(rawRestart.starCatalog(), QStringLiteral("Combined Accepted Star")));
    QVERIFY(CatalogBodyQuery::containsDisplayName(rawRestart.starCatalog(), QStringLiteral("Combined Sibling Star")));
    QCOMPARE(
        CatalogSourceFactsQuery::durableTitle(rawRestart.sourceTitles().value(instanceId)),
        QStringLiteral("Combined Accepted Title")
    );
    QCOMPARE(
        CatalogSourceFactsQuery::durableTitle(rawRestart.sourceTitles().value(siblingId)),
        QStringLiteral("Combined Sibling Title")
    );
    QVERIFY(rawRestart.constellationLineRefs().empty());
    QVERIFY(rawRestart.constellationAnchorGroups().empty());
    QCOMPARE(rawRestart.constellationCount(), std::size_t{0});
    {
        const SkyCatalogCollectionCacheLoadResult upgraded = store.loadCatalogCollectionCache();
        QVERIFY(upgraded.isLoaded());
        const SkyCatalogSourceCacheRecord* upgradedRecord =
            CatalogSourceFactsQuery::findRecord(upgraded.snapshot, instanceId);
        QVERIFY(upgradedRecord != nullptr);
        QCOMPARE(upgradedRecord->title, QStringLiteral("Combined Accepted Title"));
        QCOMPARE(upgradedRecord->version, QStringLiteral("v1"));
        QCOMPARE(upgradedRecord->urls, QStringList{acceptedUrl});
        QCOMPARE(upgradedRecord->payload, acceptedPayload);
        QCOMPARE(upgradedRecord->relatedDatasetUrls, QStringList{relatedUrl});
        QVERIFY(upgradedRecord->constellationLineRows.isEmpty());
    }
}

QTEST_GUILESS_MAIN(SkyCatalogManagerCompositionTests)

#include "SkyCatalogManagerCompositionTests.moc"
