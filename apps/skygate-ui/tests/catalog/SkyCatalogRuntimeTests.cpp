#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/SkyCatalogRuntime.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
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

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCrossIdentifiedCatalog()
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {makeHipCrossIdentifiedBody("catalog_a_1", "1", "Alpha"),
         makeHipCrossIdentifiedBody("catalog_a_2", "2", "Beta", 2.0)}
    );
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

}  // namespace

class SkyCatalogRuntimeTests final : public QObject {
    Q_OBJECT

private slots:
    void initializeBuildsActiveCatalogAndExposesSources();
    void restoreConstellationRefsUpdatesRevisionAndCount();
    void resolvedRefsTrackIdentityAndInvalidateOnSourceChange();
    void nullCatalogReportsFailureWithoutCatalogChange();
    void sourcesLoadReplaceEnableDisableAndRemoveIndependently();
    void moveSourceReordersAndClampsTarget();
    void provenanceKeepsStableIdentitiesBeyondByteRange();
    void identicalTitlesStayDistinctAndTitleChangesKeepIdentity();
    void bundledBrightStarsCarryDataProvenance();
    void bundledDeepSkyFallbackKeepsConfiguredValuesAndFillsGaps();
    void explicitReplacementSourcesKeepVisibleCollectionOrder();
    void bundledDeepSkyFallbackParticipationFollowsSourceState();
};

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

void SkyCatalogRuntimeTests::restoreConstellationRefsUpdatesRevisionAndCount()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const auto originalRevision = runtime.catalogRevision();

    const auto result = runtime.restoreConstellationRefs({{"orion", "hip_1"}}, {{"orion", {"hip_1", "hip_2"}}}, 1U);

    QVERIFY(result.catalogChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(runtime.catalogRevision() > originalRevision);
    QCOMPARE(runtime.constellationCount(), 1U);
    QCOMPARE(runtime.constellationLineRefs().size(), 1U);
    QCOMPARE(runtime.constellationAnchorGroups().size(), 1U);
}

void SkyCatalogRuntimeTests::resolvedRefsTrackIdentityAndInvalidateOnSourceChange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCrossIdentifiedCatalog());
    static_cast<void>(runtime.initialize({}));
    static_cast<void>(runtime.restoreConstellationRefs({{"hip_1", "hip_2"}}, {{"Orion", {"hip_1", "hip_2"}}}, 1U));

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

void SkyCatalogRuntimeTests::nullCatalogReportsFailureWithoutCatalogChange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(makeCatalog());

    const auto result = runtime.applySource(
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

    QVERIFY(result.statusTextChanged);
    QVERIFY(result.datasetInfoChanged);
    QVERIFY(!result.catalogChanged);
    QCOMPARE(result.statusText, QString("Catalog: Failed to load"));
    QCOMPARE(runtime.bodyCount(), 0U);
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

QTEST_APPLESS_MAIN(SkyCatalogRuntimeTests)

#include "SkyCatalogRuntimeTests.moc"
