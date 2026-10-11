#include "CatalogRuntimeFixture.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/SkyCatalogRuntime.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

using skygate::ui::tests::CatalogRuntimeFixture;

namespace {

std::unique_ptr<skygate::ephemeris::IStarCatalog> makeCatalogWithoutHipCrossIds()
{
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        {CatalogRuntimeFixture::makeFixedBody("other_1", "Other 1"),
         CatalogRuntimeFixture::makeFixedBody("other_2", "Other 2", 2.0)}
    );
}

}  // namespace

class SkyCatalogRuntimeRelatedDataTests final : public QObject {
    Q_OBJECT

private slots:
    void sourceConstellationDataIsOwnedAndComposed();
    void resolvedRefsTrackIdentityAndInvalidateOnChange();
    void ownedRelatedDataFollowsSourceLifecycle();
    void overlappingAnchorGroupsFollowVisibleOrder();
    void relatedDataDoesNotAffectOtherOwnersOrCatalogs();
    void resolvedRefsUseIdentitiesFromWinningCatalog();
    void resolvedCacheInvalidatesWhenRelatedDataChanges();
};

void SkyCatalogRuntimeRelatedDataTests::sourceConstellationDataIsOwnedAndComposed()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
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

void SkyCatalogRuntimeRelatedDataTests::resolvedRefsTrackIdentityAndInvalidateOnChange()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCrossIdentifiedCatalog());
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

void SkyCatalogRuntimeRelatedDataTests::ownedRelatedDataFollowsSourceLifecycle()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .catalogChanged
    );
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options)
            .catalogChanged
    );

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

void SkyCatalogRuntimeRelatedDataTests::overlappingAnchorGroupsFollowVisibleOrder()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .catalogChanged
    );
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options)
            .catalogChanged
    );

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

void SkyCatalogRuntimeRelatedDataTests::relatedDataDoesNotAffectOtherOwnersOrCatalogs()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCatalog());
    static_cast<void>(runtime.initialize({}));
    const skygate::ui::internal::SkyCatalogRuntimeBuildOptions options{};

    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-a"), "source_a_1", options)
            .catalogChanged
    );
    QVERIFY(
        CatalogRuntimeFixture::applySingleStarSource(runtime, QStringLiteral("source-b"), "source_b_1", options)
            .catalogChanged
    );
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

void SkyCatalogRuntimeRelatedDataTests::resolvedRefsUseIdentitiesFromWinningCatalog()
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
                        .catalog = CatalogRuntimeFixture::makeSingleStarCatalog("source_a_1", "Star A"),
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
                        .catalog = CatalogRuntimeFixture::makeHipCrossIdentifiedCatalog("source_b_1", "source_b_2"),
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

void SkyCatalogRuntimeRelatedDataTests::resolvedCacheInvalidatesWhenRelatedDataChanges()
{
    skygate::ui::internal::SkyCatalogRuntime runtime(CatalogRuntimeFixture::makeCrossIdentifiedCatalog());
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

QTEST_APPLESS_MAIN(SkyCatalogRuntimeRelatedDataTests)

#include "SkyCatalogRuntimeRelatedDataTests.moc"
