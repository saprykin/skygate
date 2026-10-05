#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/SkyCatalogRuntime.hpp"

#include <QLocale>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace {

skygate::ephemeris::OwnGalaxyCelestialBody
makeFixedBody(std::string id, std::string displayName, const double magnitude = 1.0)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.visualMagnitude = magnitude;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCatalog()
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeFixedBody("hip_1", "HIP 1"), makeFixedBody("hip_2", "HIP 2", 2.0)}
    );
}

skygate::ephemeris::OwnGalaxyCelestialBody
makeHipCrossIdentifiedBody(std::string id, std::string hip, std::string displayName, const double magnitude = 1.0)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body = makeFixedBody(std::move(id), std::move(displayName), magnitude);
    body.identity.externalIdentifiers.push_back(skygate::ephemeris::CatalogIdentifier::make("hip", std::move(hip)));
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog>
makeHipCrossIdentifiedCatalog(std::string firstId, std::string secondId)
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeHipCrossIdentifiedBody(std::move(firstId), "1", "Alpha"),
         makeHipCrossIdentifiedBody(std::move(secondId), "2", "Beta", 2.0)}
    );
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCrossIdentifiedCatalog()
{
    return makeHipCrossIdentifiedCatalog("catalog_a_1", "catalog_a_2");
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCatalogWithoutHipCrossIds()
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeFixedBody("other_1", "Other 1"), makeFixedBody("other_2", "Other 2", 2.0)}
    );
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeSingleStarCatalog(std::string id, std::string displayName)
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeFixedBody(std::move(id), std::move(displayName))}
    );
}

skygate::ui::internal::SkyCatalogRuntimeResult applySingleStarSource(
    skygate::ui::internal::SkyCatalogRuntime& runtime,
    const QString& instanceId,
    std::string bodyId,
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions& options
)
{
    return runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = instanceId,
            .title = instanceId,
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog(std::move(bodyId), "Star"),
            .foundObjectCount = 0,
        },
        options
    );
}

skygate::ephemeris::DistantCelestialBody
makeMessierBody(std::string id, std::string messierNumber, const double magnitude)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = "M" + messierNumber;
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = magnitude;
    body.identity.externalIdentifiers.push_back(
        skygate::ephemeris::CatalogIdentifier::make("messier", std::move(messierNumber))
    );
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    body.deepSkyObject =
        skygate::ephemeris::DeepSkyObjectInfo{.kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy};
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeMixedCatalog(
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<skygate::ephemeris::DistantCelestialBody> distantBodies
)
{
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyBodies.size() + distantBodies.size());
    for (std::size_t index = 0; index < ownGalaxyBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = index,
            }
        );
    }
    for (std::size_t index = 0; index < distantBodies.size(); ++index) {
        order.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant,
                .bodyIndex = index,
            }
        );
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
}

std::optional<std::size_t>
runtimeBodyIndexById(const skygate::ui::internal::SkyCatalogRuntime& runtime, const std::string& id)
{
    const skygate::ephemeris::IStarCatalog* catalog = runtime.starCatalog();
    if (catalog == nullptr) {
        return std::nullopt;
    }

    const auto bodies = catalog->bodies();
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            return index;
        }
    }
    return std::nullopt;
}

using RuntimeBuildOptions = skygate::ui::internal::SkyCatalogRuntimeBuildOptions;

RuntimeBuildOptions bundledDeepSkyOptions(const RuntimeBuildOptions::BundledDeepSkyParticipation participation)
{
    return RuntimeBuildOptions{.bundledDeepSkyParticipation = participation};
}

constexpr RuntimeBuildOptions::BundledDeepSkyParticipation kDisabledBundledDeepSky =
    RuntimeBuildOptions::BundledDeepSkyParticipation::Disabled;
constexpr RuntimeBuildOptions::BundledDeepSkyParticipation kFallbackBundledDeepSky =
    RuntimeBuildOptions::BundledDeepSkyParticipation::Fallback;

bool runtimeContainsBody(const skygate::ui::internal::SkyCatalogRuntime& runtime, const std::string& id)
{
    const skygate::ephemeris::IStarCatalog* catalog = runtime.starCatalog();
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&id](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && body->id == id;
    });
}

std::vector<QString> snapshotSourceIds(const skygate::ui::internal::SkyCatalogRuntime& runtime)
{
    const std::span<const QString> sourceIds = runtime.sourceIds();
    return std::vector<QString>(sourceIds.begin(), sourceIds.end());
}

// Returns the first violated publication invariant, or nothing when the
// runtime publishes one coherent state: the counts match the active snapshot
// and the per-body provenance arrays are parallel to its bodies with every
// winning source recorded as a contributor.
std::optional<QString> publishedStateInconsistency(const skygate::ui::internal::SkyCatalogRuntime& runtime)
{
    const skygate::ephemeris::IStarCatalog* catalog = runtime.starCatalog();
    if (catalog == nullptr) {
        return QStringLiteral("the active snapshot is missing");
    }

    const auto bodies = catalog->bodies();
    if (runtime.bodyCount() != bodies.size()) {
        return QStringLiteral("bodyCount does not match the active snapshot");
    }
    if (runtime.sourceIds().size() != bodies.size() || runtime.contributorSourceIds().size() != bodies.size()) {
        return QStringLiteral("the provenance arrays are not parallel to the active snapshot");
    }

    const std::size_t deepSkyBodyCount = static_cast<std::size_t>(
        std::count_if(bodies.begin(), bodies.end(), [](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->kind == skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
        })
    );
    if (runtime.deepSkyObjectCount() != deepSkyBodyCount) {
        return QStringLiteral("deepSkyObjectCount does not match the active snapshot");
    }

    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] == nullptr) {
            return QStringLiteral("the active snapshot holds an empty body");
        }
        const QString& winningSourceId = runtime.sourceIds()[index];
        if (winningSourceId.isEmpty()) {
            return QStringLiteral("a body has no winning source identity");
        }
        if (runtime.contributorSourceIds()[index].isEmpty()
            || !runtime.contributorSourceIds()[index].contains(winningSourceId)) {
            return QStringLiteral("a body's winning source is missing from its contributors");
        }
    }
    return std::nullopt;
}

}  // namespace

class SkyCatalogRuntimeTests final : public QObject {
    Q_OBJECT

private slots:
    void initializeBuildsActiveCatalogAndExposesSources();
    void sourceConstellationDataIsOwnedAndComposed();
    void resolvedRefsTrackIdentityAndInvalidateOnSourceChange();
    void nullCatalogPreservesLastGoodStateAndReportsFailure();
    void rejectedCollectionPreservesLastGoodState();
    void failedTransitionKeepsResolvedReferencesAndRevision();
    void successfulTransitionsPublishAlignedState();
    void sourcesLoadReplaceEnableDisableAndRemoveIndependently();
    void moveSourceReordersAndClampsTarget();
    void provenanceKeepsStableIdentitiesBeyondByteRange();
    void identicalTitlesStayDistinctAndTitleChangesKeepIdentity();
    void bundledBrightStarsCarryDataProvenance();
    void bundledDeepSkyFallbackKeepsConfiguredValuesAndFillsGaps();
    void explicitReplacementSourcesKeepVisibleCollectionOrder();
    void bundledDeepSkyFallbackParticipationFollowsSourceState();
    void statusSummarizesEnabledCollectionParticipation();
    void bundledSourceNamesComeFromActiveParticipation();
    void ownedRelatedDataFollowsSourceLifecycle();
    void overlappingAnchorGroupsFollowVisibleOrder();
    void relatedDataDoesNotAffectOtherOwnersOrCatalogs();
    void resolvedRefsUseActiveIdentitiesFromWinningCatalog();
    void resolvedCacheInvalidatesWhenRelatedDataChanges();
    void initialActivationInstallsFirstUseDefaultSource();
    void removingSoleConfiguredSourceLeavesEmptyConfiguration();
    void emptyReplacementDoesNotRecreateConfiguredSource();

private:
    // Fails the calling test when the runtime does not publish one coherent
    // state after an accepted transition.
    static void expectConsistentPublication(const skygate::ui::internal::SkyCatalogRuntime& runtime);
};

void SkyCatalogRuntimeTests::expectConsistentPublication(const skygate::ui::internal::SkyCatalogRuntime& runtime)
{
    const std::optional<QString> inconsistency = publishedStateInconsistency(runtime);
    if (inconsistency.has_value()) {
        QFAIL(qPrintable(*inconsistency));
    }
}

void SkyCatalogRuntimeTests::initializeBuildsActiveCatalogAndExposesSources()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());

    const auto result = runtime.initialize({});

    QVERIFY(result.catalogChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(runtime.starCatalog() != nullptr);
    QCOMPARE(runtime.sourceLabel(), QString("Bundled"));
    QVERIFY(runtime.bodyCount() >= 2U);
    QCOMPARE(runtime.sourceIds().size(), runtime.bodyCount());

    const auto sources = runtime.sources();
    QCOMPARE(sources.size(), std::size_t{1});
    QCOMPARE(sources[0].instanceId, QString("primary"));
    QCOMPARE(sources[0].title, QString("Bundled"));
    QCOMPARE(sources[0].policy, skygate::ephemeris::CatalogCompositionPolicy::Merge);
    QVERIFY(sources[0].enabled);
    QVERIFY(sources[0].catalog != nullptr);
}

void SkyCatalogRuntimeTests::sourceConstellationDataIsOwnedAndComposed()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const auto originalRevision = runtime.catalogRevision();

    const auto result = runtime.setSourceConstellationRefs(
        QStringLiteral("primary"), {{"orion", "hip_1"}}, {{"orion", {"hip_1", "hip_2"}}}, 1U
    );

    QVERIFY(result.catalogChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(runtime.catalogRevision() > originalRevision);
    QCOMPARE(runtime.constellationCount(), 1U);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationAnchorGroups().size(), 1U);

    // The dataset is owned by the source instance that supplied it.
    const auto sources = runtime.sources();
    QCOMPARE(sources.size(), std::size_t{1});
    QCOMPARE(sources[0].constellationData.count(), 1U);
    QCOMPARE(sources[0].constellationData.lineRefs().size(), 1U);
    QCOMPARE(sources[0].constellationData.revision(), std::uint64_t{1});

    // An unknown instance ID does not change the active view.
    const auto unknownResult =
        runtime.setSourceConstellationRefs(QStringLiteral("missing"), {{"other", "hip_1"}}, {}, 1U);
    QVERIFY(!unknownResult.catalogChanged);
    QVERIFY(!unknownResult.datasetInfoChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
}

void SkyCatalogRuntimeTests::resolvedRefsTrackIdentityAndInvalidateOnSourceChange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCrossIdentifiedCatalog());
    static_cast<void>(runtime.initialize({}));
    static_cast<void>(runtime.setSourceConstellationRefs(
        QStringLiteral("primary"), {{"hip_1", "hip_2"}}, {{"Orion", {"hip_1", "hip_2"}}}, 1U
    ));

    // The adapter-visible references keep their HIP spelling.
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QVERIFY(runtime.constellationLineRefs()[0].first == "hip_1");
    QVERIFY(runtime.constellationLineRefs()[0].second == "hip_2");

    // Consumers receive references resolved to the canonical body IDs.
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "catalog_a_1");
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].second == "catalog_a_2");
    QCOMPARE(runtime.resolvedConstellationAnchorGroups().size(), 1U);
    QVERIFY(runtime.resolvedConstellationAnchorGroups()[0].first == "Orion");
    QCOMPARE(runtime.resolvedConstellationAnchorGroups()[0].second.size(), 2U);
    QVERIFY(runtime.resolvedConstellationAnchorGroups()[0].second[0] == "catalog_a_1");
    QVERIFY(runtime.resolvedConstellationAnchorGroups()[0].second[1] == "catalog_a_2");

    // Replacing the source invalidates the previous resolutions.
    const auto revisionBeforeReplace = runtime.catalogRevision();
    const auto replaceResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("primary"),
            .title = QStringLiteral("Other"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeCatalogWithoutHipCrossIds(),
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(replaceResult.catalogChanged);
    QVERIFY(runtime.catalogRevision() > revisionBeforeReplace);
    QVERIFY(runtime.resolvedConstellationLineRefs().empty());
    QVERIFY(runtime.resolvedConstellationAnchorGroups().empty());
}

void SkyCatalogRuntimeTests::nullCatalogPreservesLastGoodStateAndReportsFailure()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    QVERIFY(runtime.initialize({}).succeeded);

    const skygate::ephemeris::IStarCatalog* const catalogBefore = runtime.starCatalog();
    const std::uint64_t revisionBefore = runtime.catalogRevision();
    const std::size_t bodyCountBefore = runtime.bodyCount();
    const QStringList sourcesBefore = runtime.sourceInstanceIds();
    const std::vector<QString> sourceIdsBefore = snapshotSourceIds(runtime);
    const QHash<QString, QString> titlesBefore = runtime.sourceTitles();
    QVERIFY(bodyCountBefore >= 2U);

    // Documented contract: a source record without a catalog is rejected as an
    // operation error. The last accepted configuration, snapshot, counts,
    // provenance, and revision stay published together instead of a cleared
    // metadata shell around an old snapshot.
    const auto replacementResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("primary"),
            .title = QStringLiteral("Broken"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = nullptr,
            .foundObjectCount = 0,
        },
        {}
    );

    QVERIFY(!replacementResult.succeeded);
    QVERIFY(replacementResult.statusTextChanged);
    QVERIFY(!replacementResult.datasetInfoChanged);
    QVERIFY(!replacementResult.deepSkyCatalogInfoChanged);
    QVERIFY(!replacementResult.catalogChanged);
    QCOMPARE(replacementResult.statusText, QString("Catalog: Failed to load"));

    QCOMPARE(runtime.starCatalog(), catalogBefore);
    QCOMPARE(runtime.catalogRevision(), revisionBefore);
    QCOMPARE(runtime.bodyCount(), bodyCountBefore);
    QCOMPARE(runtime.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(snapshotSourceIds(runtime), sourceIdsBefore);
    QCOMPARE(runtime.sourceTitles(), titlesBefore);
    QCOMPARE(runtime.sourceCount(), static_cast<std::size_t>(sourcesBefore.size()));
    const auto sources = runtime.sources();
    QCOMPARE(sources[0].title, QString("Bundled"));
    QVERIFY(sources[0].catalog != nullptr);
    expectConsistentPublication(runtime);

    // A rejected addition is not installed either.
    const auto additionResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("broken"),
            .title = QStringLiteral("Broken"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = nullptr,
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(!additionResult.succeeded);
    QVERIFY(!runtime.hasSource(QStringLiteral("broken")));
    QCOMPARE(runtime.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(runtime.catalogRevision(), revisionBefore);

    // A runtime that never accepted an activation keeps its empty published
    // state instead of inventing counts.
    skygate::ui::internal::SkyCatalogRuntime emptyRuntime(nullptr);
    const auto emptyResult = emptyRuntime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("primary"),
            .title = QStringLiteral("Broken"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = nullptr,
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(!emptyResult.succeeded);
    QCOMPARE(emptyRuntime.starCatalog(), nullptr);
    QCOMPARE(emptyRuntime.bodyCount(), 0U);
    QCOMPARE(emptyRuntime.sourceCount(), 0U);
}

void SkyCatalogRuntimeTests::rejectedCollectionPreservesLastGoodState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCrossIdentifiedCatalog());
    const RuntimeBuildOptions options{};
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).succeeded);
    QVERIFY(
        runtime.setSourceConstellationRefs(QStringLiteral("source-a"), {{"hip_1", "hip_2"}}, {{"Orion", {"hip_1"}}}, 1U)
            .succeeded
    );

    const skygate::ephemeris::IStarCatalog* const catalogBefore = runtime.starCatalog();
    const std::uint64_t revisionBefore = runtime.catalogRevision();
    const std::size_t bodyCountBefore = runtime.bodyCount();
    const QStringList sourcesBefore = runtime.sourceInstanceIds();
    const std::vector<QString> sourceIdsBefore = snapshotSourceIds(runtime);
    const QHash<QString, QString> titlesBefore = runtime.sourceTitles();
    const std::size_t constellationCountBefore = runtime.constellationCount();
    const std::size_t lineRefCountBefore = runtime.constellationLineRefs().size();
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "catalog_a_1");

    // A collection whose identities cannot compose is rejected as a whole: the
    // candidate configuration is not installed and the last accepted snapshot
    // keeps its configuration, counts, provenance, and related view.
    std::vector<skygate::ui::internal::SkyCatalogSourceRecord> duplicatedIdentity;
    duplicatedIdentity.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("duplicate"),
            .title = QStringLiteral("First"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("duplicate_1", "First"),
            .foundObjectCount = 0,
        }
    );
    duplicatedIdentity.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("duplicate"),
            .title = QStringLiteral("Second"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("duplicate_2", "Second"),
            .foundObjectCount = 0,
        }
    );
    const auto rejectedResult = runtime.replaceSources(std::move(duplicatedIdentity), options);
    QVERIFY(!rejectedResult.succeeded);
    QVERIFY(rejectedResult.statusTextChanged);
    QVERIFY(rejectedResult.statusText.startsWith(QStringLiteral("Catalog: Collection rejected")));
    QVERIFY(!rejectedResult.datasetInfoChanged);
    QVERIFY(!rejectedResult.deepSkyCatalogInfoChanged);
    QVERIFY(!rejectedResult.catalogChanged);

    QCOMPARE(runtime.starCatalog(), catalogBefore);
    QCOMPARE(runtime.catalogRevision(), revisionBefore);
    QCOMPARE(runtime.bodyCount(), bodyCountBefore);
    QCOMPARE(runtime.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(snapshotSourceIds(runtime), sourceIdsBefore);
    QCOMPARE(runtime.sourceTitles(), titlesBefore);
    QCOMPARE(runtime.constellationCount(), constellationCountBefore);
    QCOMPARE(runtime.constellationLineRefs().size(), lineRefCountBefore);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    expectConsistentPublication(runtime);

    // An added record with an empty instance identity is rejected the same way.
    const auto emptyIdentityResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QString(),
            .title = QStringLiteral("Nameless"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("nameless_1", "Nameless"),
            .foundObjectCount = 0,
        },
        options
    );
    QVERIFY(!emptyIdentityResult.succeeded);
    QVERIFY(!emptyIdentityResult.catalogChanged);
    QVERIFY(!runtime.hasSource(QString()));
    QCOMPARE(runtime.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(snapshotSourceIds(runtime), sourceIdsBefore);
    QCOMPARE(runtime.catalogRevision(), revisionBefore);

    // Operations that select nothing stay successful no-ops.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("missing"), false, options).succeeded);
    QVERIFY(runtime.removeSource(QStringLiteral("missing"), options).succeeded);
    QVERIFY(runtime.moveSource(QStringLiteral("missing"), 0U, options).succeeded);
    QCOMPARE(runtime.sourceInstanceIds(), sourcesBefore);
    QCOMPARE(runtime.catalogRevision(), revisionBefore);
}

void SkyCatalogRuntimeTests::failedTransitionKeepsResolvedReferencesAndRevision()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCrossIdentifiedCatalog());
    QVERIFY(runtime.initialize({}).succeeded);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("primary"), {{"hip_1", "hip_2"}}, {{"Orion", {"hip_1", "hip_2"}}}, 1U
                )
                .succeeded);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "catalog_a_1");
    const std::uint64_t revisionWithResolvedRefs = runtime.catalogRevision();

    // A rejected replacement reports the operation error while the resolved
    // references keep belonging to the published revision.
    const auto nullResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("primary"),
            .title = QStringLiteral("Broken"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = nullptr,
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(!nullResult.succeeded);
    QCOMPARE(runtime.catalogRevision(), revisionWithResolvedRefs);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "catalog_a_1");

    // A rejected collection replacement keeps the same snapshot and the same
    // resolved references.
    std::vector<skygate::ui::internal::SkyCatalogSourceRecord> duplicatedIdentity;
    duplicatedIdentity.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("duplicate"),
            .title = QStringLiteral("First"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("duplicate_1", "First"),
            .foundObjectCount = 0,
        }
    );
    duplicatedIdentity.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("duplicate"),
            .title = QStringLiteral("Second"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("duplicate_2", "Second"),
            .foundObjectCount = 0,
        }
    );
    const auto rejectedResult = runtime.replaceSources(std::move(duplicatedIdentity), {});
    QVERIFY(!rejectedResult.succeeded);
    QCOMPARE(runtime.catalogRevision(), revisionWithResolvedRefs);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "catalog_a_1");

    // An accepted replacement moves the revision and resolves the retained
    // references against the new snapshot instead.
    const auto replaceResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("primary"),
            .title = QStringLiteral("Other"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeHipCrossIdentifiedCatalog("other_1", "other_2"),
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(replaceResult.succeeded);
    QVERIFY(runtime.catalogRevision() > revisionWithResolvedRefs);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "other_1");
}

void SkyCatalogRuntimeTests::successfulTransitionsPublishAlignedState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    const RuntimeBuildOptions options = bundledDeepSkyOptions(kFallbackBundledDeepSky);

    std::uint64_t revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    // Replacing a source keeps the arrays aligned with the new snapshot.
    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime
                .applySource(
                    skygate::ui::internal::SkyCatalogSourceRecord{
                        .instanceId = QStringLiteral("source-a"),
                        .title = QStringLiteral("Source A"),
                        .version = QString(),
                        .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                        .enabled = true,
                        .catalog = makeSingleStarCatalog("source_a_2", "Replacement"),
                        .foundObjectCount = 0,
                    },
                    options
                )
                .succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-a"), false, options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-a"), true, options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.moveSource(QStringLiteral("source-a"), 0U, options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.removeSource(QStringLiteral("source-a"), options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.rebuildActiveCatalog(options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    // A whole-collection replacement publishes the same alignment.
    std::vector<skygate::ui::internal::SkyCatalogSourceRecord> replacement;
    replacement.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("alpha"),
            .title = QStringLiteral("Alpha"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("alpha_1", "Alpha"),
            .foundObjectCount = 0,
        }
    );
    replacement.push_back(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("beta"),
            .title = QStringLiteral("Beta"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
            .enabled = true,
            .catalog = makeMixedCatalog({}, {makeMessierBody("messier_031", "31", 1.0)}),
            .foundObjectCount = 0,
        }
    );
    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.replaceSources(std::move(replacement), options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    QCOMPARE(runtime.sourceInstanceIds(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
    expectConsistentPublication(runtime);
}

void SkyCatalogRuntimeTests::sourcesLoadReplaceEnableDisableAndRemoveIndependently()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));

    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    const auto applyStarSource = [&](const QString& instanceId, const QString& title, std::string bodyId) {
        const auto result = runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = title,
                .version = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .catalog = makeSingleStarCatalog(bodyId, "Star"),
                .foundObjectCount = 0,
            },
            options
        );
        QVERIFY(result.catalogChanged);
    };

    applyStarSource(QStringLiteral("custom-a"), QStringLiteral("Custom A"), "custom_a_1");
    applyStarSource(QStringLiteral("custom-b"), QStringLiteral("Custom B"), "custom_b_1");
    applyStarSource(QStringLiteral("custom-c"), QStringLiteral("Custom C"), "custom_c_1");

    QCOMPARE(runtime.sourceCount(), std::size_t{4});
    QCOMPARE(
        runtime.sourceInstanceIds(),
        QStringList(
            {QStringLiteral("primary"),
             QStringLiteral("custom-a"),
             QStringLiteral("custom-b"),
             QStringLiteral("custom-c")}
        )
    );
    QVERIFY(runtimeContainsBody(runtime, "hip_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_c_1"));

    // Disabling one source removes only its contribution.
    const auto disableResult = runtime.setSourceEnabled(QStringLiteral("custom-a"), false, options);
    QVERIFY(disableResult.catalogChanged);
    QVERIFY(!runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_c_1"));
    QVERIFY(runtimeContainsBody(runtime, "hip_1"));
    QVERIFY(!runtime.isSourceEnabled(QStringLiteral("custom-a")));

    // Re-enabling restores the contribution without reloading the catalog.
    const auto enableResult = runtime.setSourceEnabled(QStringLiteral("custom-a"), true, options);
    QVERIFY(enableResult.catalogChanged);
    QVERIFY(runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(runtime.isSourceEnabled(QStringLiteral("custom-a")));

    // Replacing one source keeps unrelated sources intact.
    const auto replaceResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("custom-a"),
            .title = QStringLiteral("Custom A2"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeSingleStarCatalog("custom_a_2", "Star"),
            .foundObjectCount = 0,
        },
        options
    );
    QVERIFY(replaceResult.catalogChanged);
    QVERIFY(!runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_a_2"));
    QVERIFY(runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_c_1"));

    // Removing one source leaves the remaining sources unchanged.
    const auto removeResult = runtime.removeSource(QStringLiteral("custom-b"), options);
    QVERIFY(removeResult.catalogChanged);
    QVERIFY(!runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(runtimeContainsBody(runtime, "custom_a_2"));
    QVERIFY(runtimeContainsBody(runtime, "custom_c_1"));
    QVERIFY(runtimeContainsBody(runtime, "hip_1"));
    QCOMPARE(runtime.sourceCount(), std::size_t{3});
}

void SkyCatalogRuntimeTests::moveSourceReordersAndClampsTarget()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));

    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    const auto applyStarSource = [&](const QString& instanceId, std::string bodyId) {
        const auto result = runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = instanceId,
                .version = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .catalog = makeSingleStarCatalog(bodyId, "Star"),
                .foundObjectCount = 0,
            },
            options
        );
        QVERIFY(result.catalogChanged);
    };

    applyStarSource(QStringLiteral("custom-a"), "custom_a_1");
    applyStarSource(QStringLiteral("custom-b"), "custom_b_1");
    applyStarSource(QStringLiteral("custom-c"), "custom_c_1");

    QCOMPARE(
        runtime.sourceInstanceIds(),
        QStringList(
            {QStringLiteral("primary"),
             QStringLiteral("custom-a"),
             QStringLiteral("custom-b"),
             QStringLiteral("custom-c")}
        )
    );

    const auto moveResult = runtime.moveSource(QStringLiteral("custom-c"), 1U, options);
    QVERIFY(moveResult.catalogChanged);
    QCOMPARE(
        runtime.sourceInstanceIds(),
        QStringList(
            {QStringLiteral("primary"),
             QStringLiteral("custom-c"),
             QStringLiteral("custom-a"),
             QStringLiteral("custom-b")}
        )
    );

    // Moving to an out-of-range target clamps to the last position.
    const auto clampResult = runtime.moveSource(QStringLiteral("primary"), 999U, options);
    QVERIFY(clampResult.catalogChanged);
    QCOMPARE(
        runtime.sourceInstanceIds(),
        QStringList(
            {QStringLiteral("custom-c"),
             QStringLiteral("custom-a"),
             QStringLiteral("custom-b"),
             QStringLiteral("primary")}
        )
    );

    // Moving a source to its current index does not rebuild the catalog.
    const std::uint64_t revisionBeforeNoOp = runtime.catalogRevision();
    const auto noOpResult = runtime.moveSource(QStringLiteral("custom-a"), 1U, options);
    QVERIFY(!noOpResult.catalogChanged);
    QCOMPARE(runtime.catalogRevision(), revisionBeforeNoOp);
}

void SkyCatalogRuntimeTests::provenanceKeepsStableIdentitiesBeyondByteRange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));

    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    for (int index = 0; index < 300; ++index) {
        const QString instanceId = QStringLiteral("custom-%1").arg(index);
        const auto result = runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = QStringLiteral("Source %1").arg(index),
                .version = QString(),
                .url = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .catalog = makeSingleStarCatalog("custom_" + std::to_string(index) + "_star", "Star"),
                .foundObjectCount = 0,
            },
            options
        );
        QVERIFY(result.catalogChanged);
    }

    QCOMPARE(runtime.sourceIds().size(), runtime.bodyCount());
    QVERIFY(runtime.sourceTitles().contains(QStringLiteral("custom-0")));
    QVERIFY(runtime.sourceTitles().contains(QStringLiteral("custom-299")));

    const auto bodies = runtime.starCatalog()->bodies();
    bool sawFirst = false;
    bool sawLast = false;
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const skygate::ephemeris::BaseCelestialBody* body = bodies[index];
        if (body == nullptr) {
            continue;
        }
        if (body->id == "custom_0_star") {
            sawFirst = true;
            QCOMPARE(runtime.sourceIds()[index], QStringLiteral("custom-0"));
        } else if (body->id == "custom_299_star") {
            sawLast = true;
            QCOMPARE(runtime.sourceIds()[index], QStringLiteral("custom-299"));
        }
    }
    QVERIFY(sawFirst);
    QVERIFY(sawLast);
}

void SkyCatalogRuntimeTests::identicalTitlesStayDistinctAndTitleChangesKeepIdentity()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));

    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    const auto apply = [&](const QString& instanceId, const QString& title, std::string bodyId) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = title,
                .version = QString(),
                .url = QString(),
                .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                .enabled = true,
                .catalog = makeSingleStarCatalog(std::move(bodyId), "Star"),
                .foundObjectCount = 0,
            },
            options
        );
    };

    QVERIFY(apply(QStringLiteral("custom-a"), QStringLiteral("Same title"), "custom_a_1").catalogChanged);
    QVERIFY(apply(QStringLiteral("custom-b"), QStringLiteral("Same title"), "custom_b_1").catalogChanged);

    QCOMPARE(runtime.sourceTitle(QStringLiteral("custom-a")), QStringLiteral("Same title"));
    QCOMPARE(runtime.sourceTitle(QStringLiteral("custom-b")), QStringLiteral("Same title"));
    QVERIFY(runtime.sourceTitle(QStringLiteral("custom-a")) == runtime.sourceTitle(QStringLiteral("custom-b")));

    const auto findBodyIndex = [&](const std::string& id) {
        const auto bodies = runtime.starCatalog()->bodies();
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            if (bodies[index] != nullptr && bodies[index]->id == id) {
                return static_cast<int>(index);
            }
        }
        return -1;
    };
    QCOMPARE(runtime.sourceIds()[findBodyIndex("custom_a_1")], QStringLiteral("custom-a"));
    QCOMPARE(runtime.sourceIds()[findBodyIndex("custom_b_1")], QStringLiteral("custom-b"));

    // Renaming a source must not change its stable identity.
    QVERIFY(apply(QStringLiteral("custom-a"), QStringLiteral("Renamed A"), "custom_a_2").catalogChanged);
    QCOMPARE(runtime.sourceTitle(QStringLiteral("custom-a")), QStringLiteral("Renamed A"));
    QCOMPARE(runtime.sourceIds()[findBodyIndex("custom_a_2")], QStringLiteral("custom-a"));
}

void SkyCatalogRuntimeTests::bundledBrightStarsCarryDataProvenance()
{
    skygate::ephemeris::OwnGalaxyCelestialBody constellation;
    constellation.id = "constellation_orion";
    constellation.displayName = "Orion";
    constellation.kind = skygate::ephemeris::BaseCelestialBody::Kind::Constellation;
    auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({std::move(constellation)});
    QVERIFY(catalog != nullptr);

    skygate::ui::internal::SkyCatalogRuntime runtime(std::move(catalog));
    static_cast<void>(runtime.initialize({}));

    const auto bodies = runtime.starCatalog()->bodies();
    bool sawSirius = false;
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        const skygate::ephemeris::BaseCelestialBody* body = bodies[index];
        if (body == nullptr) {
            continue;
        }
        if (body->id == "sirius") {
            sawSirius = true;
            QCOMPARE(runtime.sourceIds()[index], QStringLiteral("bundled-core"));
            QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-core")), QStringLiteral("Bundled core"));
        }
    }
    QVERIFY(sawSirius);
    QVERIFY(!runtime.sourceTitles().contains(QStringLiteral("built-in-ephemeris")));
}

void SkyCatalogRuntimeTests::bundledDeepSkyFallbackKeepsConfiguredValuesAndFillsGaps()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize(bundledDeepSkyOptions(kFallbackBundledDeepSky)));

    // A mixed configured source (stars and DSOs) keeps its own values without
    // being relabeled DeepSkyOnly to suppress the bundled fallback.
    const auto applyResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("mixed"),
            .title = QStringLiteral("Mixed"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = makeMixedCatalog(
                {makeFixedBody("mixed_star", "Mixed Star")}, {makeMessierBody("messier_031", "31", 0.0)}
            ),
            .foundObjectCount = 0,
        },
        bundledDeepSkyOptions(kFallbackBundledDeepSky)
    );
    QVERIFY(applyResult.catalogChanged);
    QVERIFY(runtimeContainsBody(runtime, "mixed_star"));

    // The configured M31 magnitude survives the falling-back bundled M31
    // (3.44): the fallback fills gaps instead of replacing later.
    const auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 0.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("mixed"));
    QCOMPARE(runtime.contributorSourceIds()[*m31Index], QStringList({QStringLiteral("mixed")}));

    // The fallback supplies the Messier identities the configured source
    // lacks, and each contribution carries its own provenance.
    const auto m1Index = runtimeBodyIndexById(runtime, "messier_001");
    QVERIFY(m1Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m1Index], QStringLiteral("bundled-deep-sky"));
    QVERIFY(runtime.sourceTitles().contains(QStringLiteral("bundled-deep-sky")));
    QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-deep-sky")), QStringLiteral("Bundled Messier"));

    // Bundled core augmentation still supplies the required Sun/Moon/planet
    // bodies that no configured source provides.
    const auto marsIndex = runtimeBodyIndexById(runtime, "mars");
    QVERIFY(marsIndex.has_value());
    QCOMPARE(runtime.sourceIds()[*marsIndex], QStringLiteral("bundled-core"));
}

void SkyCatalogRuntimeTests::explicitReplacementSourcesKeepVisibleCollectionOrder()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize(bundledDeepSkyOptions(kFallbackBundledDeepSky)));

    const auto applyM31 = [&](const QString& instanceId,
                              const skygate::ephemeris::CatalogCompositionPolicy policy,
                              const double magnitude) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = instanceId,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = makeMixedCatalog({}, {makeMessierBody("messier_031", "31", magnitude)}),
                .foundObjectCount = 0,
            },
            bundledDeepSkyOptions(kFallbackBundledDeepSky)
        );
    };

    QVERIFY(
        applyM31(QStringLiteral("merge-a"), skygate::ephemeris::CatalogCompositionPolicy::Merge, 0.0).catalogChanged
    );
    QVERIFY(
        applyM31(QStringLiteral("dso"), skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly, 5.0).catalogChanged
    );

    // The explicit later replacement source wins; the fallback never overrides
    // an explicit source's value.
    auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("dso"));

    // Moving the replacement source before the merge source restores the merge
    // source's value: the visible collection order decides.
    QVERIFY(
        runtime.moveSource(QStringLiteral("dso"), 1U, bundledDeepSkyOptions(kFallbackBundledDeepSky)).catalogChanged
    );
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 0.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("merge-a"));

    // The fallback still fills the identities no configured source supplies.
    const auto m1Index = runtimeBodyIndexById(runtime, "messier_001");
    QVERIFY(m1Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m1Index], QStringLiteral("bundled-deep-sky"));
}

void SkyCatalogRuntimeTests::bundledDeepSkyFallbackParticipationFollowsSourceState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize(bundledDeepSkyOptions(kFallbackBundledDeepSky)));

    const auto applySource = [&](const QString& instanceId,
                                 const skygate::ephemeris::CatalogCompositionPolicy policy,
                                 std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = instanceId,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = std::move(catalog),
                .foundObjectCount = 0,
            },
            bundledDeepSkyOptions(kFallbackBundledDeepSky)
        );
    };

    // An enabled explicit DSO source replaces the fallback at its visible
    // position.
    QVERIFY(applySource(
                QStringLiteral("dso"),
                skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
                makeMixedCatalog({}, {makeMessierBody("messier_031", "31", 5.0)})
    )
                .catalogChanged);
    auto m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("dso"));

    // Disabling the source lets the configured fallback fill the missing
    // identity, visible through the collection's participation information.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("dso"), false, bundledDeepSkyOptions(kFallbackBundledDeepSky))
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 3.44);
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // Re-enabling restores the explicit source's value.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("dso"), true, bundledDeepSkyOptions(kFallbackBundledDeepSky))
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.starCatalog()->bodies()[*m31Index]->visualMagnitude, 5.0);

    // Removing it lets the fallback fill the identity again.
    QVERIFY(runtime.removeSource(QStringLiteral("dso"), bundledDeepSkyOptions(kFallbackBundledDeepSky)).catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // An enabled source with no deep-sky rows cannot suppress the fallback.
    QVERIFY(applySource(
                QStringLiteral("dso-empty"),
                skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
                makeMixedCatalog({makeFixedBody("dso_empty_star", "Not Deep Sky")}, {})
    )
                .catalogChanged);
    m31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(m31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*m31Index], QStringLiteral("bundled-deep-sky"));

    // With bundled deep-sky participation disabled, nothing fills the gap.
    QVERIFY(runtime.rebuildActiveCatalog(bundledDeepSkyOptions(kDisabledBundledDeepSky)).catalogChanged);
    QVERIFY(!runtimeContainsBody(runtime, "messier_031"));
}

void SkyCatalogRuntimeTests::statusSummarizesEnabledCollectionParticipation()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    const auto options = bundledDeepSkyOptions(kDisabledBundledDeepSky);
    const auto initializeResult = runtime.initialize(options);
    QVERIFY(initializeResult.statusTextChanged);

    // One enabled configured source: the summary names it and the counts come
    // from the active snapshot. The bundled deep-sky fallback is disabled, so
    // nothing claims its data.
    QVERIFY(initializeResult.statusText.startsWith(QStringLiteral("Catalog: Bundled + Bundled core (")));
    QVERIFY(!initializeResult.statusText.contains(QStringLiteral("Bundled Messier")));

    const auto applyConfiguredSource = [&](const QString& instanceId,
                                           const QString& title,
                                           const skygate::ephemeris::CatalogCompositionPolicy policy,
                                           std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog) {
        return runtime.applySource(
            skygate::ui::internal::SkyCatalogSourceRecord{
                .instanceId = instanceId,
                .title = title,
                .version = QString(),
                .policy = policy,
                .enabled = true,
                .catalog = std::move(catalog),
                .foundObjectCount = 0,
            },
            options
        );
    };

    // Three mixed enabled sources: two star sources and one deep-sky source.
    QVERIFY(applyConfiguredSource(
                QStringLiteral("source-alpha"),
                QStringLiteral("Alpha"),
                skygate::ephemeris::CatalogCompositionPolicy::Merge,
                makeSingleStarCatalog("alpha_1", "Alpha Star")
    )
                .catalogChanged);
    QVERIFY(applyConfiguredSource(
                QStringLiteral("source-beta"),
                QStringLiteral("Beta"),
                skygate::ephemeris::CatalogCompositionPolicy::Merge,
                makeSingleStarCatalog("beta_1", "Beta Star")
    )
                .catalogChanged);
    const auto gammaResult = applyConfiguredSource(
        QStringLiteral("source-gamma"),
        QStringLiteral("Gamma"),
        skygate::ephemeris::CatalogCompositionPolicy::DeepSkyOnly,
        makeMixedCatalog({}, {makeMessierBody("messier_031", "31", 1.0)})
    );
    QVERIFY(gammaResult.catalogChanged);

    // Every enabled source is named in visible collection order, and the status
    // text carries the summary plus the snapshot counts.
    QVERIFY(runtime.participationSummary().startsWith(QStringLiteral("Bundled + Alpha + Beta + Gamma")));
    QVERIFY(gammaResult.statusText.startsWith(QStringLiteral("Catalog: %1 (").arg(runtime.participationSummary())));
    const QLocale locale = QLocale::system();
    QVERIFY(
        gammaResult.statusText.endsWith(QStringLiteral("%1 deep sky, %2 constellations)")
                                            .arg(
                                                locale.toString(static_cast<qulonglong>(runtime.deepSkyObjectCount())),
                                                locale.toString(static_cast<qulonglong>(runtime.constellationCount()))
                                            ))
    );
    QVERIFY(runtimeContainsBody(runtime, "alpha_1"));
    QVERIFY(runtimeContainsBody(runtime, "beta_1"));
    QVERIFY(runtimeContainsBody(runtime, "messier_031"));

    // Disabling a source removes it from the summary and from the counts
    // without changing the other sources' participation.
    const auto disableGammaResult = runtime.setSourceEnabled(QStringLiteral("source-gamma"), false, options);
    QVERIFY(disableGammaResult.catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled + Alpha + Beta + Bundled core"));
    QCOMPARE(runtime.deepSkyObjectCount(), 0U);
    QVERIFY(disableGammaResult.statusText.contains(QStringLiteral("0 deep sky")));
    QVERIFY(runtimeContainsBody(runtime, "alpha_1"));

    // Re-enabling restores the source's participation.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-gamma"), true, options).catalogChanged);
    QVERIFY(runtime.participationSummary().contains(QStringLiteral("Gamma")));

    // Reordering the collection reorders the summary: the collection order is
    // the precedence order.
    QVERIFY(runtime.moveSource(QStringLiteral("source-gamma"), 1U, options).catalogChanged);
    QVERIFY(runtime.participationSummary().startsWith(QStringLiteral("Bundled + Gamma + Alpha + Beta")));

    // The leading label follows the enabled collection instead of the first
    // configured record.
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Bundled"));
    QVERIFY(runtime.moveSource(QStringLiteral("source-beta"), 0U, options).catalogChanged);
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Beta"));

    // Removing a source drops its title and its objects.
    QVERIFY(runtime.removeSource(QStringLiteral("source-alpha"), options).catalogChanged);
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Alpha")));
    QVERIFY(!runtimeContainsBody(runtime, "alpha_1"));

    // With every configured source disabled only the implicit bundled
    // augmentation is left, and no disabled configured source is named.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-beta"), false, options).catalogChanged);
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-gamma"), false, options).catalogChanged);
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("primary"), false, options).catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Bundled core"));
}

void SkyCatalogRuntimeTests::bundledSourceNamesComeFromActiveParticipation()
{
    std::unique_ptr<skygate::ephemeris::IStarCatalog> bundledCatalog =
        skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(bundledCatalog != nullptr);

    skygate::ui::internal::SkyCatalogRuntime runtime(std::move(bundledCatalog));
    const auto fallbackOptions = bundledDeepSkyOptions(kFallbackBundledDeepSky);
    const auto initializeResult = runtime.initialize(fallbackOptions);
    QVERIFY(initializeResult.catalogChanged);

    // The bundled source already supplies the bundled deep-sky objects, so the
    // enabled fallback contributes nothing and is not named.
    QVERIFY(runtimeContainsBody(runtime, "messier_031"));
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(!runtime.sourceTitles().contains(QStringLiteral("bundled-deep-sky")));
    QVERIFY(!initializeResult.statusText.contains(QStringLiteral("Bundled Messier")));
    const auto bundledM31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(bundledM31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*bundledM31Index], QStringLiteral("primary"));

    // Replacing the bundled star source with a star-only catalog lets the
    // fallback supply the deep-sky objects, and the summary names it from the
    // recorded provenance instead of a catalog-name default.
    QVERIFY(runtime
                .applySource(
                    skygate::ui::internal::SkyCatalogSourceRecord{
                        .instanceId = QStringLiteral("stars"),
                        .title = QStringLiteral("Stars"),
                        .version = QString(),
                        .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                        .enabled = true,
                        .catalog = makeSingleStarCatalog("stars_1", "Configured Star"),
                        .foundObjectCount = 0,
                    },
                    fallbackOptions
                )
                .catalogChanged);
    QVERIFY(runtime.removeSource(QStringLiteral("primary"), fallbackOptions).catalogChanged);
    QCOMPARE(runtime.sourceTitle(QStringLiteral("bundled-deep-sky")), QStringLiteral("Bundled Messier"));
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Stars + Bundled core + Bundled Messier"));
    const auto fallbackM31Index = runtimeBodyIndexById(runtime, "messier_031");
    QVERIFY(fallbackM31Index.has_value());
    QCOMPARE(runtime.sourceIds()[*fallbackM31Index], QStringLiteral("bundled-deep-sky"));

    // Turning the fallback participation off drops its name together with its
    // objects.
    QVERIFY(runtime.rebuildActiveCatalog(bundledDeepSkyOptions(kDisabledBundledDeepSky)).catalogChanged);
    QVERIFY(!runtime.participationSummary().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(!runtimeContainsBody(runtime, "messier_031"));

    // A configured source the user disabled is never presented as the active
    // participant.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("stars"), false, bundledDeepSkyOptions(kDisabledBundledDeepSky))
                .catalogChanged);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
}

void SkyCatalogRuntimeTests::ownedRelatedDataFollowsSourceLifecycle()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).catalogChanged);
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options).catalogChanged);

    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-a"), {{"a_line_1", "a_line_2"}}, {{"Orion", {"a_line_1", "a_line_2"}}}, 1U
                )
                .catalogChanged);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-b"), {{"b_line_1", "b_line_2"}}, {{"Ursa", {"b_line_1", "b_line_2"}}}, 1U
                )
                .catalogChanged);

    // Two owners keep their own datasets at the same time and the active view
    // composes both.
    QCOMPARE(runtime.constellationLineRefs().size(), 2U);
    QCOMPARE(runtime.constellationAnchorGroups().size(), 2U);
    QCOMPARE(runtime.constellationCount(), 2U);

    const auto ownedData = [&runtime](const QString& instanceId) {
        for (const skygate::ui::internal::SkyCatalogSourceRecord& source : runtime.sources()) {
            if (source.instanceId == instanceId) {
                return &source.constellationData;
            }
        }
        return static_cast<const skygate::ui::internal::SkyCatalogConstellationStore*>(nullptr);
    };
    const skygate::ui::internal::SkyCatalogConstellationStore* sourceAStore = ownedData(QStringLiteral("source-a"));
    const skygate::ui::internal::SkyCatalogConstellationStore* sourceBStore = ownedData(QStringLiteral("source-b"));
    QVERIFY(sourceAStore != nullptr);
    QVERIFY(sourceBStore != nullptr);
    QCOMPARE(sourceAStore->lineRefVector().size(), 1U);
    QCOMPARE(sourceBStore->lineRefVector().size(), 1U);
    QCOMPARE(sourceAStore->lineRefVector().front().first, std::string("a_line_1"));
    QCOMPARE(sourceBStore->lineRefVector().front().first, std::string("b_line_1"));
    const std::uint64_t sourceARevision = sourceAStore->revision();
    const std::uint64_t sourceBRevision = sourceBStore->revision();

    // Disabling one owner keeps its dataset but removes it from the active
    // view without touching the other owner's data.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-a"), false, options).catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationLineRefs()[0].first, std::string("b_line_1"));
    QCOMPARE(runtime.constellationAnchorGroups().size(), 1U);
    QCOMPARE(runtime.constellationCount(), 1U);
    QCOMPARE(sourceAStore->revision(), sourceARevision);
    QCOMPARE(sourceAStore->lineRefVector().size(), 1U);
    QCOMPARE(sourceBStore->revision(), sourceBRevision);

    // Re-enabling the owner restores its contribution.
    QVERIFY(runtime.setSourceEnabled(QStringLiteral("source-a"), true, options).catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 2U);
    QCOMPARE(runtime.constellationAnchorGroups().size(), 2U);
    QCOMPARE(runtime.constellationCount(), 2U);

    // Removing one owner drops only its dataset.
    QVERIFY(runtime.removeSource(QStringLiteral("source-a"), options).catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationLineRefs()[0].first, std::string("b_line_1"));
    QCOMPARE(runtime.constellationCount(), 1U);
    QVERIFY(runtime.hasSource(QStringLiteral("source-b")));
}

void SkyCatalogRuntimeTests::overlappingAnchorGroupsFollowVisibleOrder()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).catalogChanged);
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options).catalogChanged);

    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-a"),
                    {{"a_line_1", "a_line_2"}, {"a_line_2", "a_line_3"}},
                    {{"Orion", {"a_line_1", "a_line_2"}}, {"Lyra", {"a_line_2", "a_line_3"}}},
                    3U
                )
                .catalogChanged);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-b"), {{"b_line_1", "b_line_2"}}, {{"Orion", {"b_line_1", "b_line_2"}}}, 1U
                )
                .catalogChanged);

    // The later enabled owner owns the overlapping constellation name; the
    // earlier owner's unrelated definition stays, and the declared count
    // follows the last enabled owner that declares one.
    QCOMPARE(runtime.constellationAnchorGroups().size(), 2U);
    QCOMPARE(runtime.constellationAnchorGroups()[0].first, std::string("Orion"));
    QCOMPARE(runtime.constellationAnchorGroups()[0].second.front(), std::string("b_line_1"));
    QCOMPARE(runtime.constellationAnchorGroups()[1].first, std::string("Lyra"));
    QCOMPARE(runtime.constellationCount(), 2U);

    // Reordering the collection makes the other owner's definition win.
    QVERIFY(runtime.moveSource(QStringLiteral("source-a"), 2U, options).catalogChanged);
    QCOMPARE(runtime.constellationAnchorGroups()[0].first, std::string("Orion"));
    QCOMPARE(runtime.constellationAnchorGroups()[0].second.front(), std::string("a_line_1"));
    QCOMPARE(runtime.constellationCount(), 3U);
}

void SkyCatalogRuntimeTests::relatedDataDoesNotAffectOtherOwnersOrCatalogs()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).catalogChanged);
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options).catalogChanged);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-a"), {{"a_line_1", "a_line_2"}}, {{"Orion", {"a_line_1", "a_line_2"}}}, 1U
                )
                .catalogChanged);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-b"), {{"b_line_1", "b_line_2"}}, {{"Ursa", {"b_line_1", "b_line_2"}}}, 1U
                )
                .catalogChanged);

    const std::uint64_t sourceBRevision = [&runtime]() {
        for (const skygate::ui::internal::SkyCatalogSourceRecord& source : runtime.sources()) {
            if (source.instanceId == QStringLiteral("source-b")) {
                return source.constellationData.revision();
            }
        }
        return std::uint64_t{0};
    }();

    // Replacing one owner's dataset leaves the other owner's dataset and the
    // rest of the active view untouched.
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-a"), {{"a_line_9", "a_line_10"}}, {{"Orion", {"a_line_9", "a_line_10"}}}, 1U
                )
                .catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 2U);
    QCOMPARE(runtime.constellationLineRefs()[1].first, std::string("b_line_1"));
    const auto ursaGroup = std::find_if(
        runtime.constellationAnchorGroups().begin(),
        runtime.constellationAnchorGroups().end(),
        [](const skygate::ui::internal::SkyCatalogRuntime::ConstellationAnchorGroup& anchorGroup) {
            return anchorGroup.first == "Ursa";
        }
    );
    QVERIFY(ursaGroup != runtime.constellationAnchorGroups().end());
    QCOMPARE(ursaGroup->second.front(), std::string("b_line_1"));
    for (const skygate::ui::internal::SkyCatalogSourceRecord& source : runtime.sources()) {
        if (source.instanceId == QStringLiteral("source-b")) {
            QCOMPARE(source.constellationData.revision(), sourceBRevision);
            QCOMPARE(source.constellationData.lineRefVector().front().first, std::string("b_line_1"));
        }
    }

    // Clearing one owner's dataset keeps the other owner's data active.
    QVERIFY(runtime.clearSourceConstellationRefs(QStringLiteral("source-a")).catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationLineRefs()[0].first, std::string("b_line_1"));
    QCOMPARE(runtime.constellationCount(), 1U);

    QVERIFY(!runtime.clearSourceConstellationRefs(QStringLiteral("source-a")).catalogChanged);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
}

void SkyCatalogRuntimeTests::resolvedRefsUseActiveIdentitiesFromWinningCatalog()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalogWithoutHipCrossIds());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    // The related dataset is owned by source-a, but its references only become
    // resolvable once another catalog supplies the winning HIP identities.
    QVERIFY(runtime
                .applySource(
                    skygate::ui::internal::SkyCatalogSourceRecord{
                        .instanceId = QStringLiteral("source-a"),
                        .title = QStringLiteral("Source A"),
                        .version = QString(),
                        .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                        .enabled = true,
                        .catalog = makeSingleStarCatalog("source_a_1", "Star A"),
                        .foundObjectCount = 0,
                    },
                    options
                )
                .catalogChanged);
    QVERIFY(runtime
                .setSourceConstellationRefs(
                    QStringLiteral("source-a"), {{"hip_1", "hip_2"}}, {{"Orion", {"hip_1", "hip_2"}}}, 1U
                )
                .catalogChanged);
    QVERIFY(runtime.resolvedConstellationLineRefs().empty());

    // The later source wins the HIP identities through the shared identity
    // index, so the owner's references resolve to that catalog's bodies.
    QVERIFY(runtime
                .applySource(
                    skygate::ui::internal::SkyCatalogSourceRecord{
                        .instanceId = QStringLiteral("source-b"),
                        .title = QStringLiteral("Source B"),
                        .version = QString(),
                        .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
                        .enabled = true,
                        .catalog = makeHipCrossIdentifiedCatalog("source_b_1", "source_b_2"),
                        .foundObjectCount = 0,
                    },
                    options
                )
                .catalogChanged);

    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "source_b_1");
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].second == "source_b_2");
    QCOMPARE(runtime.resolvedConstellationAnchorGroups().size(), 1U);
    QCOMPARE(runtime.resolvedConstellationAnchorGroups()[0].second.front(), std::string("source_b_1"));

    const auto bodies = runtime.starCatalog()->bodies();
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == "source_b_1") {
            QCOMPARE(runtime.sourceIds()[index], QStringLiteral("source-b"));
        }
    }
}

void SkyCatalogRuntimeTests::resolvedCacheInvalidatesWhenRelatedDataChanges()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCrossIdentifiedCatalog());
    static_cast<void>(runtime.initialize({}));
    static_cast<void>(runtime.setSourceConstellationRefs(
        QStringLiteral("primary"), {{"hip_1", "hip_2"}}, {{"Orion", {"hip_1", "hip_2"}}}, 1U
    ));
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    const std::uint64_t revisionAfterRelatedData = runtime.catalogRevision();

    // Replacing only the related dataset invalidates the resolved view even
    // though the object snapshot is untouched.
    const auto result =
        runtime.setSourceConstellationRefs(QStringLiteral("primary"), {{"hip_1", "hip_missing"}}, {}, 1U);
    QVERIFY(result.catalogChanged);
    QVERIFY(runtime.catalogRevision() > revisionAfterRelatedData);
    QVERIFY(runtime.resolvedConstellationLineRefs().empty());
    QVERIFY(runtime.resolvedConstellationAnchorGroups().empty());
}

void SkyCatalogRuntimeTests::initialActivationInstallsFirstUseDefaultSource()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(nullptr);
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    const auto result = runtime.initialize(options);
    QVERIFY(result.succeeded);
    QVERIFY(result.catalogChanged);
    QCOMPARE(runtime.sourceInstanceIds(), QStringList{QStringLiteral("primary")});
    QVERIFY(runtime.isSourceEnabled(QStringLiteral("primary")));
    QVERIFY(runtime.starCatalog() != nullptr);
    QVERIFY(runtime.bodyCount() >= 1U);

    // The first-use default is considered only by the initial activation: an
    // intentionally emptied collection is not repopulated by a later
    // initialization of the same runtime.
    QVERIFY(runtime.removeSource(QStringLiteral("primary"), options).catalogChanged);
    QCOMPARE(runtime.sourceCount(), std::size_t{0});
    QVERIFY(runtime.initialize(options).succeeded);
    QCOMPARE(runtime.sourceCount(), std::size_t{0});
    QVERIFY(runtime.sourceInstanceIds().isEmpty());
}

void SkyCatalogRuntimeTests::removingSoleConfiguredSourceLeavesEmptyConfiguration()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    QVERIFY(runtime.initialize(options).succeeded);
    QCOMPARE(runtime.sourceInstanceIds(), QStringList{QStringLiteral("primary")});

    const auto removalResult = runtime.removeSource(QStringLiteral("primary"), options);
    QVERIFY(removalResult.succeeded);
    QVERIFY(removalResult.catalogChanged);

    // The removed source is neither recreated nor replaced by an implicit
    // preset row.
    QCOMPARE(runtime.sourceCount(), std::size_t{0});
    QVERIFY(runtime.sourceInstanceIds().isEmpty());
    QVERIFY(runtime.sources().empty());
    QVERIFY(!runtime.hasSource(QStringLiteral("primary")));

    // The active snapshot is the documented bundled core augmentation, under
    // its own provenance instead of a configured source.
    QVERIFY(runtime.starCatalog() != nullptr);
    QVERIFY(runtime.bodyCount() >= 1U);
    const std::span<const QString> composedSourceIds = runtime.sourceIds();
    QCOMPARE(composedSourceIds.size(), runtime.bodyCount());
    QVERIFY(std::all_of(composedSourceIds.begin(), composedSourceIds.end(), [](const QString& sourceId) {
        return sourceId == QStringLiteral("bundled-core");
    }));
    QCOMPARE(runtime.sourceLabel(), QStringLiteral("Bundled core"));
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
    expectConsistentPublication(runtime);

    // Recomposing the accepted empty configuration does not resurrect the
    // removed source either.
    QVERIFY(runtime.rebuildActiveCatalog(options).catalogChanged);
    QCOMPARE(runtime.sourceCount(), std::size_t{0});
    QVERIFY(runtime.sourceInstanceIds().isEmpty());
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
    expectConsistentPublication(runtime);
}

void SkyCatalogRuntimeTests::emptyReplacementDoesNotRecreateConfiguredSource()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options).catalogChanged);
    QCOMPARE(runtime.sourceCount(), std::size_t{2});

    const auto replacementResult = runtime.replaceSources({}, options);
    QVERIFY(replacementResult.succeeded);
    QVERIFY(replacementResult.catalogChanged);
    QCOMPARE(runtime.sourceCount(), std::size_t{0});
    QVERIFY(runtime.sourceInstanceIds().isEmpty());
    QVERIFY(!runtime.hasSource(QStringLiteral("primary")));
    QVERIFY(!runtime.hasSource(QStringLiteral("source-a")));
    QVERIFY(runtime.starCatalog() != nullptr);
    QVERIFY(runtime.bodyCount() >= 1U);
    QCOMPARE(runtime.participationSummary(), QStringLiteral("Bundled core"));
    expectConsistentPublication(runtime);

    // The accepted empty collection is a valid base for a later addition.
    QVERIFY(applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options).catalogChanged);
    QCOMPARE(runtime.sourceInstanceIds(), QStringList{QStringLiteral("source-b")});
    expectConsistentPublication(runtime);
}

QTEST_APPLESS_MAIN(SkyCatalogRuntimeTests)

#include "SkyCatalogRuntimeTests.moc"
