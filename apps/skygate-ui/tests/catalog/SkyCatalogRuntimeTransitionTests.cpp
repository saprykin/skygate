#include "CatalogRuntimeFixture.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogRuntime.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using skygate::ui::tests::CatalogRuntimeFixture;

namespace {

using RuntimeBuildOptions = skygate::ui::internal::SkyCatalogRuntimeBuildOptions;

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

class SkyCatalogRuntimeTransitionTests final : public QObject {
    Q_OBJECT

private slots:
    void initializeBuildsActiveCatalogAndExposesSources();
    void nullCatalogPreservesLastGoodStateAndReportsFailure();
    void rejectedCollectionPreservesLastGoodState();
    void failedTransitionKeepsResolvedReferencesAndRevision();
    void successfulTransitionsPublishAlignedState();
    void sourcesLoadReplaceEnableDisableAndRemoveIndependently();
    void moveSourceReordersAndClampsTarget();
    void provenanceKeepsStableIdentitiesBeyondByteRange();
    void identicalTitlesStayDistinctAndTitleChangesKeepIdentity();
    void initialActivationInstallsFirstUseDefaultSource();
    void removingSoleConfiguredSourceLeavesEmptyConfiguration();
    void emptyReplacementDoesNotRecreateConfiguredSource();

private:
    // Fails the calling test when the runtime does not publish one coherent
    // state after an accepted transition.
    static void expectConsistentPublication(const skygate::ui::internal::SkyCatalogRuntime& runtime);
};

void SkyCatalogRuntimeTransitionTests::expectConsistentPublication(
    const skygate::ui::internal::SkyCatalogRuntime& runtime
)
{
    const std::optional<QString> inconsistency = publishedStateInconsistency(runtime);
    if (inconsistency.has_value()) {
        QFAIL(qPrintable(*inconsistency));
    }
}

void SkyCatalogRuntimeTransitionTests::initializeBuildsActiveCatalogAndExposesSources()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());

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

void SkyCatalogRuntimeTransitionTests::nullCatalogPreservesLastGoodStateAndReportsFailure()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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

void SkyCatalogRuntimeTransitionTests::rejectedCollectionPreservesLastGoodState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCrossIdentifiedCatalog());
    const RuntimeBuildOptions options{};
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .succeeded
    );
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("duplicate_1", "First"),
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("duplicate_2", "Second"),
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("nameless_1", "Nameless"),
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

void SkyCatalogRuntimeTransitionTests::failedTransitionKeepsResolvedReferencesAndRevision()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCrossIdentifiedCatalog());
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("duplicate_1", "First"),
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("duplicate_2", "Second"),
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
            .catalog = CatalogRuntimeFixture::makeHipCrossIdentifiedCatalog("other_1", "other_2"),
            .foundObjectCount = 0,
        },
        {}
    );
    QVERIFY(replaceResult.succeeded);
    QVERIFY(runtime.catalogRevision() > revisionWithResolvedRefs);
    QCOMPARE(runtime.resolvedConstellationLineRefs().size(), 1U);
    QVERIFY(runtime.resolvedConstellationLineRefs()[0].first == "other_1");
}

void SkyCatalogRuntimeTransitionTests::successfulTransitionsPublishAlignedState()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    const RuntimeBuildOptions options =
        CatalogRuntimeFixture::bundledDeepSkyOptions(CatalogRuntimeFixture::kFallbackBundledDeepSky);

    std::uint64_t revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    expectConsistentPublication(runtime);

    revisionBefore = runtime.catalogRevision();
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .succeeded
    );
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
                        .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("source_a_2", "Replacement"),
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
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("alpha_1", "Alpha"),
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
            .catalog = CatalogRuntimeFixture::makeMixedCatalog(
                {}, {CatalogRuntimeFixture::makeMessierBody("messier_031", "31", 1.0)}
            ),
            .foundObjectCount = 0,
        }
    );
    revisionBefore = runtime.catalogRevision();
    QVERIFY(runtime.replaceSources(std::move(replacement), options).succeeded);
    QVERIFY(runtime.catalogRevision() > revisionBefore);
    QCOMPARE(runtime.sourceInstanceIds(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
    expectConsistentPublication(runtime);
}

void SkyCatalogRuntimeTransitionTests::sourcesLoadReplaceEnableDisableAndRemoveIndependently()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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
                .catalog = CatalogRuntimeFixture::makeSingleStarCatalog(bodyId, "Star"),
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
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "hip_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_c_1"));

    // Disabling one source removes only its contribution.
    const auto disableResult = runtime.setSourceEnabled(QStringLiteral("custom-a"), false, options);
    QVERIFY(disableResult.catalogChanged);
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_c_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "hip_1"));
    QVERIFY(!runtime.isSourceEnabled(QStringLiteral("custom-a")));

    // Re-enabling restores the contribution without reloading the catalog.
    const auto enableResult = runtime.setSourceEnabled(QStringLiteral("custom-a"), true, options);
    QVERIFY(enableResult.catalogChanged);
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(runtime.isSourceEnabled(QStringLiteral("custom-a")));

    // Replacing one source keeps unrelated sources intact.
    const auto replaceResult = runtime.applySource(
        skygate::ui::internal::SkyCatalogSourceRecord{
            .instanceId = QStringLiteral("custom-a"),
            .title = QStringLiteral("Custom A2"),
            .version = QString(),
            .policy = skygate::ephemeris::CatalogCompositionPolicy::Merge,
            .enabled = true,
            .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("custom_a_2", "Star"),
            .foundObjectCount = 0,
        },
        options
    );
    QVERIFY(replaceResult.catalogChanged);
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_2"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_c_1"));

    // Removing one source leaves the remaining sources unchanged.
    const auto removeResult = runtime.removeSource(QStringLiteral("custom-b"), options);
    QVERIFY(removeResult.catalogChanged);
    QVERIFY(!CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_b_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_a_2"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "custom_c_1"));
    QVERIFY(CatalogRuntimeFixture::runtimeContainsBody(runtime, "hip_1"));
    QCOMPARE(runtime.sourceCount(), std::size_t{3});
}

void SkyCatalogRuntimeTransitionTests::moveSourceReordersAndClampsTarget()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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
                .catalog = CatalogRuntimeFixture::makeSingleStarCatalog(bodyId, "Star"),
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

void SkyCatalogRuntimeTransitionTests::provenanceKeepsStableIdentitiesBeyondByteRange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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
                .catalog =
                    CatalogRuntimeFixture::makeSingleStarCatalog("custom_" + std::to_string(index) + "_star", "Star"),
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

void SkyCatalogRuntimeTransitionTests::identicalTitlesStayDistinctAndTitleChangesKeepIdentity()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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
                .catalog = CatalogRuntimeFixture::makeSingleStarCatalog(std::move(bodyId), "Star"),
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

void SkyCatalogRuntimeTransitionTests::initialActivationInstallsFirstUseDefaultSource()
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

void SkyCatalogRuntimeTransitionTests::removingSoleConfiguredSourceLeavesEmptyConfiguration()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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

void SkyCatalogRuntimeTransitionTests::emptyReplacementDoesNotRecreateConfiguredSource()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};
    QVERIFY(runtime.initialize(options).succeeded);
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .catalogChanged
    );
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
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options)
            .catalogChanged
    );
    QCOMPARE(runtime.sourceInstanceIds(), QStringList{QStringLiteral("source-b")});
    expectConsistentPublication(runtime);
}

QTEST_APPLESS_MAIN(SkyCatalogRuntimeTransitionTests)

#include "SkyCatalogRuntimeTransitionTests.moc"
