#include "TestHelpers.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/CatalogObjectIdentity.hpp"

#include <QByteArray>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogBinaryCodec;
using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ephemeris::CatalogCompositionRequest;
using skygate::ephemeris::CatalogCompositionResult;
using skygate::ephemeris::CatalogCompositionSourceEntry;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogLoadResult;
using skygate::ephemeris::CatalogObjectIdentity;
using skygate::ephemeris::IStarCatalog;

// The two anonymous HYG payloads: neither row carries an id,
// hip, or any other recognized designation, so each parse generates its own
// hyg_auto_1 record key.
constexpr std::string_view kAnonymousPayloadA = "ra,dec,mag\n1,2,3\n";
constexpr std::string_view kAnonymousPayloadB = "ra,dec,mag\n10,20,4\n";

constexpr std::string_view kMatchingHipPayloadA = "hip,ra,dec,mag\n123,1,2,3\n";
constexpr std::string_view kMatchingHipPayloadB = "hip,ra,dec,mag\n123,10,20,4\n";

constexpr std::string_view kSourceA = "payload-a";
constexpr std::string_view kSourceB = "payload-b";

CatalogLoadResult loadHyg(const std::string_view payload)
{
    return skygate::ephemeris::CatalogLoader::load(skygate::ephemeris::CatalogSourceType::HygCsv, payload);
}

CatalogCompositionResult compose(const std::vector<CatalogCompositionSourceEntry>& sources)
{
    CatalogCompositionRequest request;
    request.sources = sources;
    return skygate::ephemeris::CatalogComposer::composeCollection(request);
}

CatalogCompositionResult composeSingle(const std::string_view sourceId, const IStarCatalog& catalog)
{
    return compose({
        {.sourceId = std::string(sourceId),
         .enabled = true,
         .catalog = &catalog,
         .policy = CatalogCompositionPolicy::Merge},
    });
}

std::vector<std::string> bodyIds(const CatalogCompositionResult& result)
{
    std::vector<std::string> ids;
    for (const BaseCelestialBody* body : result.catalog->bodies()) {
        if (body != nullptr) {
            ids.push_back(body->id);
        }
    }
    return ids;
}

bool hasIdentifier(const BaseCelestialBody& body, const std::string_view namespaceName, const std::string_view value)
{
    return std::any_of(
        body.identity.externalIdentifiers.begin(),
        body.identity.externalIdentifiers.end(),
        [&](const CatalogIdentifier& identifier) {
            return identifier.namespaceName == namespaceName && identifier.value == value;
        }
    );
}

}  // namespace

class CatalogSourceLocalIdentityTests final : public QObject {
    Q_OBJECT

private slots:
    void keepsAnonymousHygPayloadsFromIndependentSourcesDistinct();
    void keepsEqualLocalRecordNumbersFromUnrelatedSourcesDistinct();
    void keepsObjectKeysStableAcrossSourceReloads();
    void keepsSourceLocalScopeThroughBinarySerialization();
    void mergesMatchingHipIdentifiersAcrossSources();
    void rejectsEmptySourceIdentity();
    void rejectsDuplicateSourceIdentity();
};

void CatalogSourceLocalIdentityTests::keepsAnonymousHygPayloadsFromIndependentSourcesDistinct()
{
    const CatalogLoadResult sourceA = loadHyg(kAnonymousPayloadA);
    const CatalogLoadResult sourceB = loadHyg(kAnonymousPayloadB);
    QVERIFY2(sourceA.isSuccess(), sourceA.errorDetail.c_str());
    QVERIFY2(sourceB.isSuccess(), sourceB.errorDetail.c_str());

    const CatalogCompositionResult result = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceA.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceB),
         .enabled = true,
         .catalog = sourceB.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{2});
    QCOMPARE(result.starCount, std::size_t{2});

    const auto bodies = result.catalog->bodies();
    const BaseCelestialBody* first = skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1@payload-a");
    const BaseCelestialBody* second = skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1@payload-b");
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(first->fixedEquatorialValue().has_value());
    QVERIFY(second->fixedEquatorialValue().has_value());
    QCOMPARE(first->fixedEquatorialValue()->rightAscensionHours, 1.0);
    QCOMPARE(first->fixedEquatorialValue()->declinationDeg, 2.0);
    QCOMPARE(second->fixedEquatorialValue()->rightAscensionHours, 10.0);
    QCOMPARE(second->fixedEquatorialValue()->declinationDeg, 20.0);
}

void CatalogSourceLocalIdentityTests::keepsEqualLocalRecordNumbersFromUnrelatedSourcesDistinct()
{
    // Two parses of the same payload produce the same hyg_auto_1 counter, the
    // same coordinates, and the same display name. Only the owning source
    // instance may distinguish them; a position- or name-derived key would
    // wrongly collapse the two objects.
    const CatalogLoadResult firstParse = loadHyg(kAnonymousPayloadA);
    const CatalogLoadResult secondParse = loadHyg(kAnonymousPayloadA);
    QVERIFY2(firstParse.isSuccess(), firstParse.errorDetail.c_str());
    QVERIFY2(secondParse.isSuccess(), secondParse.errorDetail.c_str());

    const CatalogCompositionResult result = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = firstParse.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceB),
         .enabled = true,
         .catalog = secondParse.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{2});

    const auto bodies = result.catalog->bodies();
    const BaseCelestialBody* first = skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1@payload-a");
    const BaseCelestialBody* second = skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1@payload-b");
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QCOMPARE(QString::fromStdString(first->displayName), QStringLiteral("hyg_auto_1"));
    QCOMPARE(QString::fromStdString(second->displayName), QStringLiteral("hyg_auto_1"));

    // The bare parser counter alone must not resolve to either object.
    QVERIFY(skygate::ephemeris::tests::findBodyById(bodies, "hyg_auto_1") == nullptr);
}

void CatalogSourceLocalIdentityTests::keepsObjectKeysStableAcrossSourceReloads()
{
    const CatalogLoadResult initialLoad = loadHyg(kAnonymousPayloadA);
    const CatalogLoadResult reloaded = loadHyg(kAnonymousPayloadA);
    const CatalogLoadResult otherSource = loadHyg(kAnonymousPayloadB);
    QVERIFY2(initialLoad.isSuccess(), initialLoad.errorDetail.c_str());
    QVERIFY2(reloaded.isSuccess(), reloaded.errorDetail.c_str());
    QVERIFY2(otherSource.isSuccess(), otherSource.errorDetail.c_str());

    const CatalogCompositionResult initial = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = initialLoad.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceB),
         .enabled = true,
         .catalog = otherSource.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });
    const CatalogCompositionResult afterReload = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = reloaded.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceB),
         .enabled = true,
         .catalog = otherSource.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(initial.isSuccess());
    QVERIFY(afterReload.isSuccess());
    QCOMPARE(initial.bodyCount, std::size_t{2});
    QCOMPARE(afterReload.bodyCount, initial.bodyCount);

    const std::vector<std::string> expectedIds = {"hyg_auto_1@payload-a", "hyg_auto_1@payload-b"};
    QCOMPARE(bodyIds(initial), expectedIds);
    QCOMPARE(bodyIds(afterReload), expectedIds);
}

void CatalogSourceLocalIdentityTests::keepsSourceLocalScopeThroughBinarySerialization()
{
    const CatalogLoadResult parsed = loadHyg(kAnonymousPayloadA);
    QVERIFY2(parsed.isSuccess(), parsed.errorDetail.c_str());
    QCOMPARE(parsed.catalog->bodies().size(), std::size_t{1});
    QCOMPARE(parsed.catalog->bodies()[0]->identity.idScope, CatalogObjectIdentity::IdScope::SourceLocal);

    const QByteArray payload = CatalogBinaryCodec::serialize(parsed.catalog->catalog());
    QVERIFY(!payload.isEmpty());
    const std::unique_ptr<IStarCatalog> restored = CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const auto restoredBodies = restored->catalog().ownGalaxyBodies();
    QCOMPARE(restoredBodies.size(), std::size_t{1});
    QCOMPARE(restoredBodies[0].identity.idScope, CatalogObjectIdentity::IdScope::SourceLocal);

    const CatalogCompositionResult direct = composeSingle(kSourceA, *parsed.catalog);
    const CatalogCompositionResult afterRestore = composeSingle(kSourceA, *restored);
    QVERIFY(direct.isSuccess());
    QVERIFY(afterRestore.isSuccess());

    const std::vector<std::string> expectedIds = {"hyg_auto_1@payload-a"};
    QCOMPARE(bodyIds(direct), expectedIds);
    QCOMPARE(bodyIds(afterRestore), expectedIds);

    // The composed key is instance-qualified and therefore global.
    const BaseCelestialBody* composed =
        skygate::ephemeris::tests::findBodyById(afterRestore.catalog->bodies(), "hyg_auto_1@payload-a");
    QVERIFY(composed != nullptr);
    QCOMPARE(composed->identity.idScope, CatalogObjectIdentity::IdScope::Global);
}

void CatalogSourceLocalIdentityTests::mergesMatchingHipIdentifiersAcrossSources()
{
    const CatalogLoadResult sourceA = loadHyg(kMatchingHipPayloadA);
    const CatalogLoadResult sourceB = loadHyg(kMatchingHipPayloadB);
    QVERIFY2(sourceA.isSuccess(), sourceA.errorDetail.c_str());
    QVERIFY2(sourceB.isSuccess(), sourceB.errorDetail.c_str());

    const CatalogCompositionResult result = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceA.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceB),
         .enabled = true,
         .catalog = sourceB.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodyCount, std::size_t{1});
    QCOMPARE(result.starCount, std::size_t{1});

    const BaseCelestialBody* merged = skygate::ephemeris::tests::findBodyById(result.catalog->bodies(), "hip_123");
    QVERIFY(merged != nullptr);
    QVERIFY(hasIdentifier(*merged, "hip", "123"));
    QVERIFY(merged->fixedEquatorialValue().has_value());
    QCOMPARE(merged->fixedEquatorialValue()->rightAscensionHours, 10.0);

    QCOMPARE(result.sourceIds.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(result.sourceIds[0]), QStringLiteral("payload-b"));
    QCOMPARE(result.contributorSourceIds.size(), std::size_t{1});
    QCOMPARE(result.contributorSourceIds[0].size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(result.contributorSourceIds[0][0]), QStringLiteral("payload-b"));
    QCOMPARE(QString::fromStdString(result.contributorSourceIds[0][1]), QStringLiteral("payload-a"));
}

void CatalogSourceLocalIdentityTests::rejectsEmptySourceIdentity()
{
    const CatalogLoadResult source = loadHyg(kAnonymousPayloadA);
    QVERIFY2(source.isSuccess(), source.errorDetail.c_str());

    QTest::ignoreMessage(QtWarningMsg, "catalog composition rejected source 0: the source identity is empty.");
    const CatalogCompositionResult result = compose({
        {.sourceId = "", .enabled = true, .catalog = source.catalog.get(), .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(!result.isSuccess());
    QVERIFY(result.catalog == nullptr);
    QCOMPARE(result.errorCode, CatalogCompositionResult::ErrorCode::InvalidSourceIdentity);
    QCOMPARE(
        QString::fromStdString(result.errorDetail),
        QStringLiteral("catalog composition rejected source 0: the source identity is empty.")
    );
}

void CatalogSourceLocalIdentityTests::rejectsDuplicateSourceIdentity()
{
    const CatalogLoadResult sourceA = loadHyg(kAnonymousPayloadA);
    const CatalogLoadResult sourceB = loadHyg(kAnonymousPayloadB);
    QVERIFY2(sourceA.isSuccess(), sourceA.errorDetail.c_str());
    QVERIFY2(sourceB.isSuccess(), sourceB.errorDetail.c_str());

    QTest::ignoreMessage(
        QtWarningMsg,
        "catalog composition rejected source 1: source identity 'payload-a' duplicates another source in the "
        "collection."
    );
    const CatalogCompositionResult duplicate = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceA.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceB.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(!duplicate.isSuccess());
    QCOMPARE(duplicate.errorCode, CatalogCompositionResult::ErrorCode::InvalidSourceIdentity);
    QVERIFY(duplicate.errorDetail.find("payload-a") != std::string::npos);

    // Identities that differ only by case resolve to the same composed key,
    // so they are duplicate source identities as well.
    QTest::ignoreMessage(
        QtWarningMsg,
        "catalog composition rejected source 1: source identity 'Payload-A' duplicates another source in the "
        "collection."
    );
    const CatalogCompositionResult caseVariant = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceA.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "Payload-A",
         .enabled = true,
         .catalog = sourceB.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(!caseVariant.isSuccess());
    QCOMPARE(caseVariant.errorCode, CatalogCompositionResult::ErrorCode::InvalidSourceIdentity);
    QVERIFY(caseVariant.errorDetail.find("Payload-A") != std::string::npos);

    // Surrounding whitespace is removed by identity-key normalization too, so
    // a padded identity is the same source identity.
    QTest::ignoreMessage(
        QtWarningMsg,
        "catalog composition rejected source 1: source identity 'payload-a ' duplicates another source in the "
        "collection."
    );
    const CatalogCompositionResult paddedVariant = compose({
        {.sourceId = std::string(kSourceA),
         .enabled = true,
         .catalog = sourceA.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
        {.sourceId = "payload-a ",
         .enabled = true,
         .catalog = sourceB.catalog.get(),
         .policy = CatalogCompositionPolicy::Merge},
    });

    QVERIFY(!paddedVariant.isSuccess());
    QCOMPARE(paddedVariant.errorCode, CatalogCompositionResult::ErrorCode::InvalidSourceIdentity);
    QVERIFY(paddedVariant.errorDetail.find("payload-a ") != std::string::npos);
}

QTEST_APPLESS_MAIN(CatalogSourceLocalIdentityTests)

#include "CatalogSourceLocalIdentityTests.moc"
