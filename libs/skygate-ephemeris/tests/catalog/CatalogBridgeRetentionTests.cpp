#include "BaseCelestialBody.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogCompositionResult.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogComposer;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::IStarCatalog;
using skygate::ephemeris::tests::CatalogMergeTestSupport;

class CatalogBridgeRetentionTests final : public QObject {
    Q_OBJECT

private slots:
    void bridgesIdentifiersAcquiredEarlierInTheSameSourcePass();
    void bridgesAmbiguousAuthoritativeIdentifiersIntoSingleSurvivor();
    void keepsIncompatibleKindsDistinctWhileBridgingSameKindSurvivors();
    void bridgesDeepSkyObjectsWithMetadataUnion();
    void multiStageBridgesRetainEveryContributorOnce();
    void retainsReplacedCanonicalIdentityAcrossLaterSources();
    void retainsBridgedCanonicalIdentitiesAcrossLaterSources();
    void retainedCanonicalIdentitySurvivesBinaryRoundTripAndRecomposition();
    void combinedChainKeepsIdentityMetadataAndProvenanceAcrossSnapshotRecomposition();
};

void CatalogBridgeRetentionTests::bridgesIdentifiersAcquiredEarlierInTheSameSourcePass()
{
    // Source A establishes that HIP 1 and HD 2 are one object. Source B's
    // first record replaces A and acquires HD 2; its later record resolves
    // through that acquired identifier instead of appending a second survivor.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "a_hip1_hd2",
            "Established",
            {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")},
            {},
            1.0,
            2.0
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {
            CatalogMergeTestSupport::makeStar("b_hip1", {}, {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0),
            CatalogMergeTestSupport::makeStar(
                "b_hd2",
                "Later record",
                {CatalogIdentifier::make("hd", "2"), CatalogIdentifier::make("hyg", "5")},
                {},
                1.0,
                2.0
            ),
        },
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "a_hip1_hd2"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "b_hd2"), std::size_t{0});

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(bodies, "b_hip1");
    QVERIFY(winner != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "hd", "2"));
    // b_hd2 was merged into the survivor, so its own identifier survives too.
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "hyg", "5"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Established"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-a"}));
}

void CatalogBridgeRetentionTests::bridgesAmbiguousAuthoritativeIdentifiersIntoSingleSurvivor()
{
    // The bridging record carries both authoritative identifiers. It resolves
    // each of them to a different survivor of an earlier source, so it becomes
    // the single survivor of all three records instead of a third object.
    //
    // Contract correction: the bridge carries no display name of
    // its own, so its missing name comes from the highest-priority contributor,
    // and contributors are listed from the winner down to the lowest-priority
    // source. Source precedence is the configured collection position, not the
    // accumulator position of an absorbed survivor: source-c is configured
    // after source-a, so `Gamma` wins over `Alpha` and source-c precedes
    // source-a. The earlier version of this test asserted accumulator position
    // order, which is not source precedence.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Alpha", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("c_hd2", "Gamma", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "b_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceC != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "a_hip1"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "c_hd2"), std::size_t{0});

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(bodies, "b_bridge");
    QVERIFY(winner != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "hd", "2"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("Gamma"));
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-c", "source-a"}));
}

void CatalogBridgeRetentionTests::keepsIncompatibleKindsDistinctWhileBridgingSameKindSurvivors()
{
    const auto stars = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("star_a", {}, {CatalogIdentifier::make("hip", "123")}, {}, 1.0, 2.0)}, {}
    );
    const auto conflictingDeepSky = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "dso_hip_123", "Conflicting DSO", {}, {CatalogIdentifier::make("hip", "123")}, 1.0, 2.0
        )}
    );
    const auto starBridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("star_bridge", {}, {CatalogIdentifier::make("hip", "123")}, {}, 1.0, 2.0)},
        {}
    );
    QVERIFY(stars != nullptr);
    QVERIFY(conflictingDeepSky != nullptr);
    QVERIFY(starBridge != nullptr);

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept dso_hip_123 distinct from star_a because the shared identity is used by "
        "incompatible object kinds."
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog composition kept star_bridge distinct from dso_hip_123 because the shared identity is used by "
        "incompatible object kinds."
    );

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "stars", .enabled = true, .catalog = stars.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "conflicting-deep-sky",
         .enabled = true,
         .catalog = conflictingDeepSky.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "star-bridge",
         .enabled = true,
         .catalog = starBridge.get(),
         .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{2});
    QCOMPARE(result.starCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "star_a"), std::size_t{0});

    const BaseCelestialBody* bridgedStar = CatalogMergeTestSupport::findBodyById(bodies, "star_bridge");
    QVERIFY(bridgedStar != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*bridgedStar, "hip", "123"));
    const BaseCelestialBody* conflicting = CatalogMergeTestSupport::findBodyById(bodies, "dso_hip_123");
    QVERIFY(conflicting != nullptr);
    QCOMPARE(conflicting->kind, BaseCelestialBody::Kind::DeepSkyObject);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*conflicting, "hip", "123"));
}

void CatalogBridgeRetentionTests::bridgesDeepSkyObjectsWithMetadataUnion()
{
    // The bridge carries no display name and no major axis of its own, so the
    // missing deep-sky metadata comes from the highest-priority contributor.
    // Contract correction: source-c is configured after source-a,
    // so `C Triangulum` and its major axis win over `A Andromeda` and 178.0,
    // and source-c precedes source-a in the contributor list. Identifiers and
    // aliases still union from every contributor.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "a_ngc_224", "A Andromeda", {"Andromeda Galaxy"}, {CatalogIdentifier::make("ngc", "224")}, 1.0, 2.0, 178.0
        )}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "c_messier_31", "C Triangulum", {"Triangulum"}, {CatalogIdentifier::make("messier", "31")}, 1.0, 2.0, 60.0
        )}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {},
        {CatalogMergeTestSupport::makeDeepSkyObject(
            "b_bridge",
            {},
            {},
            {CatalogIdentifier::make("ngc", "224"), CatalogIdentifier::make("messier", "31")},
            std::nullopt,
            std::nullopt,
            std::nullopt
        )}
    );
    QVERIFY(sourceA != nullptr);
    QVERIFY(sourceC != nullptr);
    QVERIFY(sourceB != nullptr);

    CatalogCompositionRequest request;
    request.sources = {
        {.sourceId = "source-a", .enabled = true, .catalog = sourceA.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-c", .enabled = true, .catalog = sourceC.get(), .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "source-b", .enabled = true, .catalog = sourceB.get(), .policy = CatalogCompositionPolicy::Merge},
    };

    const CatalogCompositionResult result = CatalogComposer::composeCollection(request);

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.deepSkyObjectCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "a_ngc_224"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "c_messier_31"), std::size_t{0});

    const BaseCelestialBody* winner = CatalogMergeTestSupport::findBodyById(bodies, "b_bridge");
    QVERIFY(winner != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "ngc", "224"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*winner, "messier", "031"));
    QCOMPARE(QString::fromStdString(winner->displayName), QStringLiteral("C Triangulum"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(winner->identity.aliases, "Andromeda Galaxy"));
    QVERIFY(CatalogMergeTestSupport::hasAlias(winner->identity.aliases, "Triangulum"));
    QVERIFY(winner->fixedEquatorialValue().has_value());
    QCOMPARE(winner->fixedEquatorialValue()->rightAscensionHours, 1.0);
    const auto* deepSkyInfo = winner->deepSkyObjectInfo();
    QVERIFY(deepSkyInfo != nullptr);
    QVERIFY(deepSkyInfo->majorAxisArcmin.has_value());
    QCOMPARE(*deepSkyInfo->majorAxisArcmin, 60.0);
    QCOMPARE(result.sourceIds.front(), std::string("source-b"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-b", "source-c", "source-a"}));
}

void CatalogBridgeRetentionTests::multiStageBridgesRetainEveryContributorOnce()
{
    // A second bridge absorbs the first bridge's survivor together with one
    // more source. Every contributing source appears exactly once, ordered
    // from the second bridge's source down to the earliest source, and the
    // higher-priority contributor supplies the missing name.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto firstBridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceD = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("d_hyg3", "D", {CatalogIdentifier::make("hyg", "3")}, {}, 1.0, 2.0)}, {}
    );
    const auto secondBridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "e_bridge", {}, {CatalogIdentifier::make("hd", "2"), CatalogIdentifier::make("hyg", "3")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult result = CatalogMergeTestSupport::composeAll(
        {sourceA.get(), sourceB.get(), firstBridge.get(), sourceD.get(), secondBridge.get()}
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "c_bridge"), std::size_t{0});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "d_hyg3"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "e_bridge");
    QVERIFY(survivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hyg", "3"));
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("D"));
    QCOMPARE(result.sourceIds.front(), std::string("source-4"));
    QCOMPARE(
        result.contributorSourceIds.front(),
        (std::vector<std::string>{"source-4", "source-3", "source-2", "source-1", "source-0"})
    );
}

void CatalogBridgeRetentionTests::retainsReplacedCanonicalIdentityAcrossLaterSources()
{
    // A and B establish that both canonical keys identify the same object
    // because they carry the same HIP cross-identifier. B replaces A, then C
    // arrives carrying A's canonical id without any cross-identifier. The
    // established equivalence must resolve C to the survivor instead of
    // appending a duplicate object.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "original", "Original A", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "replacement", "Replacement B", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceC = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("original", "Late C", {}, {}, 1.0, 2.0)}, {}
    );

    const CatalogCompositionResult result =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceC.get()});

    QVERIFY(result.isSuccess());
    const std::span<const BaseCelestialBody* const> bodies = result.catalog->bodies();
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "original"), std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(bodies, "replacement"), std::size_t{0});

    const BaseCelestialBody* survivor = CatalogMergeTestSupport::findBodyById(bodies, "original");
    QVERIFY(survivor != nullptr);
    // The later record C wins the public identity and inherits the absorbed
    // cross-identifier.
    QCOMPARE(QString::fromStdString(survivor->displayName), QStringLiteral("Late C"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*survivor, "hip", "99"));
    // The replaced canonical id stays authoritative identity data, never a
    // display alias.
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*survivor, "replacement"));
    QCOMPARE(survivor->identity.retainedCanonicalIds.size(), std::size_t{1});
    QVERIFY(!CatalogMergeTestSupport::hasAlias(survivor->identity.aliases, "replacement"));
    QCOMPARE(result.sourceIds.front(), std::string("source-2"));
    QCOMPARE(result.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));
}

void CatalogBridgeRetentionTests::retainsBridgedCanonicalIdentitiesAcrossLaterSources()
{
    // A bridge absorbs every same-kind survivor it resolves, and later
    // replacements keep every absorbed canonical id resolvable, including ids
    // absorbed in an earlier composition stage.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", {}, {CatalogIdentifier::make("hip", "1")})}, {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", {}, {CatalogIdentifier::make("hd", "2")})}, {}
    );
    const auto sourceBridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}
        )},
        {}
    );
    const auto sourceReplacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("d_hd2", "Later D", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceOriginal = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Latest A", {}, {}, 1.0, 2.0)}, {}
    );

    const CatalogCompositionResult bridged =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});
    QVERIFY(bridged.isSuccess());
    QCOMPARE(bridged.bodyCount, std::size_t{1});
    const BaseCelestialBody* bridgeSurvivor =
        CatalogMergeTestSupport::findBodyById(bridged.catalog->bodies(), "c_bridge");
    QVERIFY(bridgeSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*bridgeSurvivor, "a_hip1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*bridgeSurvivor, "b_hd2"));
    QCOMPARE(bridgeSurvivor->identity.retainedCanonicalIds.size(), std::size_t{2});

    const CatalogCompositionResult replaced = CatalogMergeTestSupport::composeAll(
        {sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get()}
    );
    QVERIFY(replaced.isSuccess());
    QCOMPARE(replaced.bodyCount, std::size_t{1});
    const BaseCelestialBody* replacementSurvivor =
        CatalogMergeTestSupport::findBodyById(replaced.catalog->bodies(), "d_hd2");
    QVERIFY(replacementSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*replacementSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*replacementSurvivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacementSurvivor, "c_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacementSurvivor, "a_hip1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*replacementSurvivor, "b_hd2"));
    QCOMPARE(replacementSurvivor->identity.retainedCanonicalIds.size(), std::size_t{3});

    // A row carrying a canonical id absorbed two stages earlier still resolves
    // to the single survivor instead of appending a fourth object.
    const CatalogCompositionResult lateOriginal = CatalogMergeTestSupport::composeAll(
        {sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get(), sourceOriginal.get()}
    );
    QVERIFY(lateOriginal.isSuccess());
    QCOMPARE(lateOriginal.bodyCount, std::size_t{1});
    const BaseCelestialBody* finalSurvivor =
        CatalogMergeTestSupport::findBodyById(lateOriginal.catalog->bodies(), "a_hip1");
    QVERIFY(finalSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(finalSurvivor->displayName), QStringLiteral("Latest A"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*finalSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*finalSurvivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*finalSurvivor, "d_hd2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*finalSurvivor, "c_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*finalSurvivor, "b_hd2"));
}

void CatalogBridgeRetentionTests::retainedCanonicalIdentitySurvivesBinaryRoundTripAndRecomposition()
{
    // The established equivalence survives binary
    // serialization, index reconstruction, and reuse of the already-composed
    // snapshot as a composition input.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "original", "Original", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "replacement", "Replacement", {CatalogIdentifier::make("hip", "99")}, {}, 1.0, 2.0
        )},
        {}
    );

    const CatalogCompositionResult composed = CatalogMergeTestSupport::composeInOrder(*sourceA, *sourceB);
    QVERIFY(composed.isSuccess());
    QCOMPARE(composed.bodyCount, std::size_t{1});
    const BaseCelestialBody* composedSurvivor =
        CatalogMergeTestSupport::findBodyById(composed.catalog->bodies(), "replacement");
    QVERIFY(composedSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*composedSurvivor, "original"));

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(composed.catalog->catalog());
    QVERIFY(!payload.isEmpty());
    const std::unique_ptr<IStarCatalog> restored = skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const BaseCelestialBody* restoredSurvivor =
        CatalogMergeTestSupport::findBodyById(restored->bodies(), "replacement");
    QVERIFY(restoredSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*restoredSurvivor, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*restoredSurvivor, "original"));

    const auto laterOriginal = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("original", "Late Original", {}, {}, 1.0, 2.0)}, {}
    );

    // The row carrying the earlier canonical id resolves to the restored
    // survivor whether it arrives before or after the restored snapshot, so
    // the rebuilt index resolves retained keys on both sides.
    const CatalogCompositionResult restoredFirst = CatalogMergeTestSupport::composeInOrder(*restored, *laterOriginal);
    QVERIFY(restoredFirst.isSuccess());
    QCOMPARE(restoredFirst.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(restoredFirst.catalog->bodies(), "replacement"), std::size_t{0});
    const BaseCelestialBody* lateWinner =
        CatalogMergeTestSupport::findBodyById(restoredFirst.catalog->bodies(), "original");
    QVERIFY(lateWinner != nullptr);
    QCOMPARE(QString::fromStdString(lateWinner->displayName), QStringLiteral("Late Original"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*lateWinner, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*lateWinner, "replacement"));

    const CatalogCompositionResult restoredLast = CatalogMergeTestSupport::composeInOrder(*laterOriginal, *restored);
    QVERIFY(restoredLast.isSuccess());
    QCOMPARE(restoredLast.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(restoredLast.catalog->bodies(), "original"), std::size_t{0});
    const BaseCelestialBody* snapshotWinner =
        CatalogMergeTestSupport::findBodyById(restoredLast.catalog->bodies(), "replacement");
    QVERIFY(snapshotWinner != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*snapshotWinner, "hip", "99"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*snapshotWinner, "original"));

    // The composed snapshot used directly as a composition input behaves the
    // same way as the serialized one.
    const CatalogCompositionResult composedAsInput =
        CatalogMergeTestSupport::composeInOrder(*laterOriginal, *composed.catalog);
    QVERIFY(composedAsInput.isSuccess());
    QCOMPARE(composedAsInput.bodyCount, std::size_t{1});
    const BaseCelestialBody* composedWinner =
        CatalogMergeTestSupport::findBodyById(composedAsInput.catalog->bodies(), "replacement");
    QVERIFY(composedWinner != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*composedWinner, "original"));
}

void CatalogBridgeRetentionTests::combinedChainKeepsIdentityMetadataAndProvenanceAcrossSnapshotRecomposition()
{
    // A three-source chain whose bridge resolves two name owners, followed by a
    // replacement that carries the bridge's second key. The bridge inherits its
    // missing display name from the highest-precedence contributor, and the
    // replacement absorbs the bridge while keeping every earlier canonical id
    // resolvable. Composing the earlier chain through its binary snapshot must
    // reach the same survivor identity, metadata, and contributor accounting as
    // composing all four sources in one pass.
    const auto sourceA = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Earlier A", {CatalogIdentifier::make("hip", "1")}, {}, 1.0, 2.0)},
        {}
    );
    const auto sourceB = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("b_hd2", "Later B", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );
    const auto sourceBridge = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar(
            "c_bridge", {}, {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0
        )},
        {}
    );
    const auto sourceReplacement = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("d_hd2", "Latest D", {CatalogIdentifier::make("hd", "2")}, {}, 1.0, 2.0)}, {}
    );

    const CatalogCompositionResult direct = CatalogMergeTestSupport::composeAll(
        {sourceA.get(), sourceB.get(), sourceBridge.get(), sourceReplacement.get()}
    );
    QVERIFY(direct.isSuccess());
    QCOMPARE(direct.bodyCount, std::size_t{1});
    const BaseCelestialBody* directSurvivor = CatalogMergeTestSupport::findBodyById(direct.catalog->bodies(), "d_hd2");
    QVERIFY(directSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(directSurvivor->displayName), QStringLiteral("Latest D"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*directSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*directSurvivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*directSurvivor, "c_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*directSurvivor, "a_hip1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*directSurvivor, "b_hd2"));
    QCOMPARE(
        direct.contributorSourceIds.front(), (std::vector<std::string>{"source-3", "source-2", "source-1", "source-0"})
    );

    // The earlier chain on its own: one bridge survivor that inherited its
    // missing name from source B, the highest-precedence contributor, and that
    // accounts for all three contributors once.
    const CatalogCompositionResult bridgeChain =
        CatalogMergeTestSupport::composeAll({sourceA.get(), sourceB.get(), sourceBridge.get()});
    QVERIFY(bridgeChain.isSuccess());
    QCOMPARE(bridgeChain.bodyCount, std::size_t{1});
    const BaseCelestialBody* bridgeSurvivor =
        CatalogMergeTestSupport::findBodyById(bridgeChain.catalog->bodies(), "c_bridge");
    QVERIFY(bridgeSurvivor != nullptr);
    QCOMPARE(QString::fromStdString(bridgeSurvivor->displayName), QStringLiteral("Later B"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*bridgeSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*bridgeSurvivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*bridgeSurvivor, "a_hip1"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*bridgeSurvivor, "b_hd2"));
    QCOMPARE(bridgeSurvivor->identity.retainedCanonicalIds.size(), std::size_t{2});
    QCOMPARE(bridgeChain.contributorSourceIds.front(), (std::vector<std::string>{"source-2", "source-1", "source-0"}));

    const QByteArray payload = skygate::ephemeris::CatalogBinaryCodec::serialize(bridgeChain.catalog->catalog());
    QVERIFY(!payload.isEmpty());
    const std::unique_ptr<IStarCatalog> restored = skygate::ephemeris::CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    // Recomposing the restored snapshot with the same later replacement must
    // resolve to one survivor instead of appending the replacement as a second
    // object, so identity, metadata, and retained canonical ids stay equal to
    // the single-pass composition.
    const CatalogCompositionResult snapshotChain =
        CatalogMergeTestSupport::composeAll({restored.get(), sourceReplacement.get()});
    QVERIFY(snapshotChain.isSuccess());
    QCOMPARE(snapshotChain.bodyCount, std::size_t{1});
    QCOMPARE(CatalogMergeTestSupport::countBodiesById(snapshotChain.catalog->bodies(), "d_hd2"), std::size_t{1});
    const BaseCelestialBody* snapshotSurvivor =
        CatalogMergeTestSupport::findBodyById(snapshotChain.catalog->bodies(), "d_hd2");
    QVERIFY(snapshotSurvivor != nullptr);
    QCOMPARE(snapshotSurvivor->id, directSurvivor->id);
    QCOMPARE(snapshotSurvivor->displayName, directSurvivor->displayName);
    QCOMPARE(snapshotSurvivor->kind, directSurvivor->kind);
    QCOMPARE(
        CatalogMergeTestSupport::sortedIdentifierKeys(*snapshotSurvivor),
        CatalogMergeTestSupport::sortedIdentifierKeys(*directSurvivor)
    );
    QCOMPARE(
        CatalogMergeTestSupport::sortedRetainedCanonicalIds(*snapshotSurvivor),
        CatalogMergeTestSupport::sortedRetainedCanonicalIds(*directSurvivor)
    );
    QVERIFY(snapshotSurvivor->fixedEquatorialValue().has_value());
    QCOMPARE(
        snapshotSurvivor->fixedEquatorialValue()->rightAscensionHours,
        directSurvivor->fixedEquatorialValue()->rightAscensionHours
    );
    QCOMPARE(
        snapshotSurvivor->fixedEquatorialValue()->declinationDeg, directSurvivor->fixedEquatorialValue()->declinationDeg
    );

    // Provenance stays a complete accounting at each boundary: the chain names
    // its three contributors once, and the recomposition names the snapshot and
    // the later replacement once each.
    QCOMPARE(snapshotChain.sourceIds.front(), std::string("source-1"));
    QCOMPARE(snapshotChain.contributorSourceIds.front(), (std::vector<std::string>{"source-1", "source-0"}));

    // The canonical id that survived the replacement inside the snapshot still
    // resolves when a row carrying it arrives after serialization.
    const auto lateOriginal = CatalogMergeTestSupport::createCatalog(
        {CatalogMergeTestSupport::makeStar("a_hip1", "Late A", {}, {}, 1.0, 2.0)}, {}
    );
    const CatalogCompositionResult lateResult =
        CatalogMergeTestSupport::composeAll({restored.get(), lateOriginal.get()});
    QVERIFY(lateResult.isSuccess());
    QCOMPARE(lateResult.bodyCount, std::size_t{1});
    const BaseCelestialBody* lateSurvivor =
        CatalogMergeTestSupport::findBodyById(lateResult.catalog->bodies(), "a_hip1");
    QVERIFY(lateSurvivor != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*lateSurvivor, "hip", "1"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*lateSurvivor, "hd", "2"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*lateSurvivor, "c_bridge"));
    QVERIFY(CatalogMergeTestSupport::hasRetainedCanonicalId(*lateSurvivor, "b_hd2"));
}

QTEST_APPLESS_MAIN(CatalogBridgeRetentionTests)

#include "CatalogBridgeRetentionTests.moc"
