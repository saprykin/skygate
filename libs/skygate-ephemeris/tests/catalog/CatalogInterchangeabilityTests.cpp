#include "CatalogArchiveTestSupport.hpp"
#include "CatalogSemanticFixtureAdapter.hpp"
#include "CatalogSemanticFixtureCorpus.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogBinaryCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogLoader.hpp"
#include "catalog/CatalogPayloadParser.hpp"
#include "catalog/constellation/ConstellationReferenceResolver.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogStarAstrometry;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

constexpr double kRaToleranceHours = 1e-3;
constexpr double kDecToleranceDeg = 1e-2;
constexpr double kExtentToleranceArcmin = 1e-3;

bool nearEqual(const double lhs, const double rhs, const double tolerance)
{
    return std::abs(lhs - rhs) <= tolerance;
}

bool sameOptionalDouble(const std::optional<double>& lhs, const std::optional<double>& rhs, const double tolerance)
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    return !lhs.has_value() || nearEqual(*lhs, *rhs, tolerance);
}

bool sameCoordinate(
    const std::optional<skygate::core::EquatorialCoordinate>& lhs,
    const std::optional<skygate::core::EquatorialCoordinate>& rhs
)
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    return nearEqual(lhs->rightAscensionHours, rhs->rightAscensionHours, kRaToleranceHours)
           && nearEqual(lhs->declinationDeg, rhs->declinationDeg, kDecToleranceDeg);
}

bool sameIdentifiers(const std::vector<CatalogIdentifier>& lhs, const std::vector<CatalogIdentifier>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::all_of(lhs.begin(), lhs.end(), [&rhs](const CatalogIdentifier& identifier) {
        return std::find(rhs.begin(), rhs.end(), identifier) != rhs.end();
    });
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

bool hasAliasIgnoreCase(const std::vector<std::string>& aliases, const std::string_view alias)
{
    return std::any_of(aliases.begin(), aliases.end(), [&](const std::string& existing) {
        return skygate::ephemeris::StringUtilities::equalsIgnoreAsciiCase(existing, alias);
    });
}

bool sameAstrometry(const std::optional<CatalogStarAstrometry>& lhs, const std::optional<CatalogStarAstrometry>& rhs)
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    if (!nearEqual(
            lhs->referenceEquatorial.rightAscensionHours,
            rhs->referenceEquatorial.rightAscensionHours,
            kRaToleranceHours
        )
        || !nearEqual(
            lhs->referenceEquatorial.declinationDeg, rhs->referenceEquatorial.declinationDeg, kDecToleranceDeg
        )) {
        return false;
    }
    if (lhs->referenceEpoch != rhs->referenceEpoch) {
        return false;
    }
    return sameOptionalDouble(
               lhs->properMotionRightAscensionMasPerYear, rhs->properMotionRightAscensionMasPerYear, 1e-3
           )
           && sameOptionalDouble(lhs->properMotionDeclinationMasPerYear, rhs->properMotionDeclinationMasPerYear, 1e-3)
           && sameOptionalDouble(lhs->stellarParallaxMas, rhs->stellarParallaxMas, 1e-3)
           && sameOptionalDouble(lhs->radialVelocityKmPerSecond, rhs->radialVelocityKmPerSecond, 1e-3);
}

bool sameDeepSkyInfo(const std::optional<DeepSkyObjectInfo>& lhs, const std::optional<DeepSkyObjectInfo>& rhs)
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    if (lhs->kind != rhs->kind) {
        return false;
    }
    return sameOptionalDouble(lhs->majorAxisArcmin, rhs->majorAxisArcmin, kExtentToleranceArcmin)
           && sameOptionalDouble(lhs->minorAxisArcmin, rhs->minorAxisArcmin, kExtentToleranceArcmin)
           && sameOptionalDouble(lhs->positionAngleDeg, rhs->positionAngleDeg, kExtentToleranceArcmin);
}

// Compares consumer-visible body fields while allowing the canonical ID and
// source-record ID to differ by schema. External identifiers must match.
[[nodiscard]] std::string verifySchemaEquivalent(const BaseCelestialBody& lhs, const BaseCelestialBody& rhs)
{
    if (lhs.kind != rhs.kind) {
        return "kind differs";
    }
    if (!nearEqual(lhs.visualMagnitude, rhs.visualMagnitude, 1e-9)) {
        return "magnitude differs";
    }
    if (lhs.displayName != rhs.displayName) {
        return "displayName differs";
    }
    if (!sameCoordinate(lhs.fixedEquatorialValue(), rhs.fixedEquatorialValue())) {
        return "fixed coordinates differ";
    }
    if (!sameIdentifiers(lhs.identity.externalIdentifiers, rhs.identity.externalIdentifiers)) {
        return "external identifiers differ";
    }
    if (lhs.kind == BaseCelestialBody::Kind::Star
        && !sameAstrometry(lhs.starAstrometryValue(), rhs.starAstrometryValue())) {
        return "star astrometry differs";
    }
    if (lhs.kind == BaseCelestialBody::Kind::DeepSkyObject
        && !sameDeepSkyInfo(lhs.deepSkyObjectValue(), rhs.deepSkyObjectValue())) {
        return "deep-sky metadata differs";
    }
    return {};
}

const BaseCelestialBody* findByExternalIdentifier(
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view namespaceName,
    const std::string_view value
)
{
    const auto it = std::find_if(bodies.begin(), bodies.end(), [&](const BaseCelestialBody* body) {
        return body != nullptr && hasIdentifier(*body, namespaceName, value);
    });
    return it == bodies.end() ? nullptr : *it;
}

std::size_t
countBodiesOfKind(const std::span<const BaseCelestialBody* const> bodies, const BaseCelestialBody::Kind kind)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [kind](const BaseCelestialBody* body) {
        return body != nullptr && body->kind == kind;
    }));
}

std::optional<std::string> sourceIdFor(
    const skygate::ephemeris::CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view id
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            if (index >= result.sourceIds.size()) {
                return std::nullopt;
            }
            return result.sourceIds[index];
        }
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> contributorsFor(
    const skygate::ephemeris::CatalogCompositionResult& result,
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view id
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            if (index >= result.contributorSourceIds.size()) {
                return std::nullopt;
            }
            return result.contributorSourceIds[index];
        }
    }
    return std::nullopt;
}

// Splits a parsed catalog into a catalog of a single body kind using the
// authoritative body copy operation.
std::unique_ptr<skygate::ephemeris::IStarCatalog>
filterBodies(const skygate::ephemeris::IStarCatalog& source, const BaseCelestialBody::Kind kind)
{
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    for (const BaseCelestialBody* body : source.bodies()) {
        if (body == nullptr || body->kind != kind) {
            continue;
        }
        if (kind == BaseCelestialBody::Kind::DeepSkyObject) {
            order.push_back(
                skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                    .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant,
                    .bodyIndex = distantBodies.size(),
                }
            );
            distantBodies.push_back(skygate::ephemeris::CelestialBodyCatalog::copyDistantBody(*body));
        } else {
            order.push_back(
                skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                    .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                    .bodyIndex = ownGalaxyBodies.size(),
                }
            );
            ownGalaxyBodies.push_back(skygate::ephemeris::CelestialBodyCatalog::copyOwnGalaxyBody(*body));
        }
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
}

}  // namespace

class CatalogInterchangeabilityTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void equivalentObjectsAgreeAcrossSchemas();
    void equivalentObjectsSurviveContainerVariants();
    void equivalentSourcesComposeDeterministically();
    void cacheRoundTripPreservesEquivalentMetadata();
    void malformedInputsFailConsistently();
    void largeFixtureIndexesAndResolvesReferences();
};

void CatalogInterchangeabilityTests::initTestCase()
{
    // Register the deliberately different test schema once for this process.
    QVERIFY(
        skygate::ephemeris::tests::CatalogSemanticFixtureAdapter::schemaType()
        != skygate::ephemeris::CatalogSourceType::Unknown
    );
}

void CatalogInterchangeabilityTests::equivalentObjectsAgreeAcrossSchemas()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());

    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());
    QCOMPARE(hyg.detectedFormat, CatalogSourceType::HygCsv);
    QCOMPARE(openNgc.detectedFormat, CatalogSourceType::OpenNgcCsv);
    QCOMPARE(semantic.detectedFormat, CatalogSemanticFixtureAdapter::schemaType());

    const std::span<const BaseCelestialBody* const> hygBodies = hyg.catalog->bodies();
    const std::span<const BaseCelestialBody* const> openNgcBodies = openNgc.catalog->bodies();
    const std::span<const BaseCelestialBody* const> semanticBodies = semantic.catalog->bodies();

    QCOMPARE(hygBodies.size(), std::size_t{2});
    QCOMPARE(openNgcBodies.size(), std::size_t{2});
    QCOMPARE(semanticBodies.size(), std::size_t{4});

    const BaseCelestialBody* hygSirius = findByExternalIdentifier(hygBodies, "hip", "32349");
    const BaseCelestialBody* semanticSirius = findByExternalIdentifier(semanticBodies, "hip", "32349");
    QVERIFY(hygSirius != nullptr);
    QVERIFY(semanticSirius != nullptr);
    const std::string siriusError = verifySchemaEquivalent(*hygSirius, *semanticSirius);
    QVERIFY2(siriusError.empty(), siriusError.c_str());
    QVERIFY(hasAliasIgnoreCase(semanticSirius->identity.aliases, "Alpha Canis Majoris"));

    const BaseCelestialBody* hygVega = findByExternalIdentifier(hygBodies, "hip", "91262");
    const BaseCelestialBody* semanticVega = findByExternalIdentifier(semanticBodies, "hip", "91262");
    QVERIFY(hygVega != nullptr);
    QVERIFY(semanticVega != nullptr);
    const std::string vegaError = verifySchemaEquivalent(*hygVega, *semanticVega);
    QVERIFY2(vegaError.empty(), vegaError.c_str());

    const BaseCelestialBody* openNgcAndromeda = findByExternalIdentifier(openNgcBodies, "messier", "031");
    const BaseCelestialBody* semanticAndromeda = findByExternalIdentifier(semanticBodies, "messier", "031");
    QVERIFY(openNgcAndromeda != nullptr);
    QVERIFY(semanticAndromeda != nullptr);
    const std::string andromedaError = verifySchemaEquivalent(*openNgcAndromeda, *semanticAndromeda);
    QVERIFY2(andromedaError.empty(), andromedaError.c_str());
    QVERIFY(hasAliasIgnoreCase(semanticAndromeda->identity.aliases, "Andromeda Galaxy"));

    const BaseCelestialBody* openNgcRing = findByExternalIdentifier(openNgcBodies, "messier", "057");
    const BaseCelestialBody* semanticRing = findByExternalIdentifier(semanticBodies, "messier", "057");
    QVERIFY(openNgcRing != nullptr);
    QVERIFY(semanticRing != nullptr);
    const std::string ringError = verifySchemaEquivalent(*openNgcRing, *semanticRing);
    QVERIFY2(ringError.empty(), ringError.c_str());
    QVERIFY(hasAliasIgnoreCase(semanticRing->identity.aliases, "Ring Nebula"));
}

void CatalogInterchangeabilityTests::equivalentObjectsSurviveContainerVariants()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const std::string zipData = tests::makeZip({
        tests::ZipEntrySpec{
            .path = "semantic.tsv", .data = std::string(CatalogSemanticFixtureCorpus::semanticPayload())
        },
    });

    const CatalogPayloadParser parser;
    const auto plain = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    const auto gzip = parser.parseResult(CatalogSemanticFixtureCorpus::semanticGzipPayload());
    const auto zip = parser.parseResult(zipData);

    QVERIFY2(plain.isSuccess(), plain.errorDetail.c_str());
    QVERIFY2(gzip.isSuccess(), gzip.errorDetail.c_str());
    QVERIFY2(zip.isSuccess(), zip.errorDetail.c_str());

    const auto schemaType = CatalogSemanticFixtureAdapter::schemaType();
    QCOMPARE(plain.detectedFormat, schemaType);
    QCOMPARE(gzip.detectedFormat, schemaType);
    QCOMPARE(zip.detectedFormat, schemaType);

    const std::span<const BaseCelestialBody* const> plainBodies = plain.catalog->bodies();
    const std::span<const BaseCelestialBody* const> gzipBodies = gzip.catalog->bodies();
    const std::span<const BaseCelestialBody* const> zipBodies = zip.catalog->bodies();

    QCOMPARE(plainBodies.size(), std::size_t{4});
    QCOMPARE(gzipBodies.size(), std::size_t{4});
    QCOMPARE(zipBodies.size(), std::size_t{4});

    for (std::size_t index = 0; index < plainBodies.size(); ++index) {
        const std::string gzipError = verifySchemaEquivalent(*plainBodies[index], *gzipBodies[index]);
        QVERIFY2(gzipError.empty(), gzipError.c_str());
        const std::string zipError = verifySchemaEquivalent(*plainBodies[index], *zipBodies[index]);
        QVERIFY2(zipError.empty(), zipError.c_str());
    }
}

void CatalogInterchangeabilityTests::equivalentSourcesComposeDeterministically()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    auto semanticStars = filterBodies(*semantic.catalog, BaseCelestialBody::Kind::Star);
    auto semanticDsos = filterBodies(*semantic.catalog, BaseCelestialBody::Kind::DeepSkyObject);
    QVERIFY(semanticStars != nullptr);
    QVERIFY(semanticDsos != nullptr);

    const auto compose = [&](const bool semanticFirst) {
        CatalogCompositionRequest request;
        if (semanticFirst) {
            request.sources = {
                {.sourceId = "semantic-stars",
                 .enabled = true,
                 .catalog = semanticStars.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "hyg",
                 .enabled = true,
                 .catalog = hyg.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "semantic-dsos",
                 .enabled = true,
                 .catalog = semanticDsos.get(),
                 .policy = CatalogCompositionPolicy::DeepSkyOnly},
                {.sourceId = "openngc",
                 .enabled = true,
                 .catalog = openNgc.catalog.get(),
                 .policy = CatalogCompositionPolicy::DeepSkyOnly},
            };
        } else {
            request.sources = {
                {.sourceId = "hyg",
                 .enabled = true,
                 .catalog = hyg.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "semantic-stars",
                 .enabled = true,
                 .catalog = semanticStars.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "openngc",
                 .enabled = true,
                 .catalog = openNgc.catalog.get(),
                 .policy = CatalogCompositionPolicy::DeepSkyOnly},
                {.sourceId = "semantic-dsos",
                 .enabled = true,
                 .catalog = semanticDsos.get(),
                 .policy = CatalogCompositionPolicy::DeepSkyOnly},
            };
        }
        return CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult semanticLast = compose(false);
    QVERIFY(semanticLast.isSuccess());
    {
        const std::span<const BaseCelestialBody* const> bodies = semanticLast.catalog->bodies();
        QCOMPARE(countBodiesOfKind(bodies, BaseCelestialBody::Kind::Star), std::size_t{2});
        QCOMPARE(countBodiesOfKind(bodies, BaseCelestialBody::Kind::DeepSkyObject), std::size_t{2});

        // Later semantic star wins and absorbs the HYG record's identifiers.
        const BaseCelestialBody* sirius = findByExternalIdentifier(bodies, "hip", "32349");
        QVERIFY(sirius != nullptr);
        QCOMPARE(QString::fromStdString(sirius->id), QStringLiteral("star_1"));
        QVERIFY(hasIdentifier(*sirius, "hip", "32349"));
        QVERIFY(hasIdentifier(*sirius, "hyg", "1"));
        QCOMPARE(*sourceIdFor(semanticLast, bodies, sirius->id), std::string("semantic-stars"));
        const auto siriusContributors = contributorsFor(semanticLast, bodies, sirius->id);
        QVERIFY(siriusContributors.has_value());
        QCOMPARE(*siriusContributors, (std::vector<std::string>{"semantic-stars", "hyg"}));

        // Later semantic DSO wins and absorbs the OpenNGC aliases.
        const BaseCelestialBody* andromeda = findByExternalIdentifier(bodies, "messier", "031");
        QVERIFY(andromeda != nullptr);
        QCOMPARE(QString::fromStdString(andromeda->id), QStringLiteral("dso_3"));
        QVERIFY(hasAliasIgnoreCase(andromeda->identity.aliases, "Andromeda Galaxy"));
        QVERIFY(hasAliasIgnoreCase(andromeda->identity.aliases, "PGC 2557"));
        QVERIFY(hasAliasIgnoreCase(andromeda->identity.aliases, "M 31"));
        QCOMPARE(*sourceIdFor(semanticLast, bodies, andromeda->id), std::string("semantic-dsos"));
        const auto andromedaContributors = contributorsFor(semanticLast, bodies, andromeda->id);
        QVERIFY(andromedaContributors.has_value());
        QCOMPARE(*andromedaContributors, (std::vector<std::string>{"semantic-dsos", "openngc"}));
    }

    // Reversing the source order flips the deterministic winners.
    const CatalogCompositionResult semanticFirst = compose(true);
    QVERIFY(semanticFirst.isSuccess());
    {
        const std::span<const BaseCelestialBody* const> bodies = semanticFirst.catalog->bodies();
        const BaseCelestialBody* sirius = findByExternalIdentifier(bodies, "hip", "32349");
        QVERIFY(sirius != nullptr);
        QCOMPARE(QString::fromStdString(sirius->id), QStringLiteral("hip_32349"));
        QCOMPARE(*sourceIdFor(semanticFirst, bodies, sirius->id), std::string("hyg"));

        const BaseCelestialBody* andromeda = findByExternalIdentifier(bodies, "messier", "031");
        QVERIFY(andromeda != nullptr);
        QCOMPARE(QString::fromStdString(andromeda->id), QStringLiteral("messier_031"));
        QCOMPARE(*sourceIdFor(semanticFirst, bodies, andromeda->id), std::string("openngc"));
    }
}

void CatalogInterchangeabilityTests::cacheRoundTripPreservesEquivalentMetadata()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    const QByteArray payload = CatalogBinaryCodec::serialize(semantic.catalog->catalog());
    QVERIFY(!payload.isEmpty());

    std::unique_ptr<IStarCatalog> restored = CatalogBinaryCodec::deserialize(payload);
    QVERIFY(restored != nullptr);

    const std::span<const BaseCelestialBody* const> originalBodies = semantic.catalog->bodies();
    const std::span<const BaseCelestialBody* const> restoredBodies = restored->bodies();
    QCOMPARE(originalBodies.size(), restoredBodies.size());

    for (std::size_t index = 0; index < originalBodies.size(); ++index) {
        const BaseCelestialBody& lhs = *originalBodies[index];
        const BaseCelestialBody& rhs = *restoredBodies[index];
        QCOMPARE(QString::fromStdString(lhs.id), QString::fromStdString(rhs.id));
        QCOMPARE(QString::fromStdString(lhs.displayName), QString::fromStdString(rhs.displayName));
        QCOMPARE(static_cast<int>(lhs.kind), static_cast<int>(rhs.kind));
        QVERIFY(nearEqual(lhs.visualMagnitude, rhs.visualMagnitude, 1e-9));
        QCOMPARE(
            QString::fromStdString(lhs.identity.sourceRecordId), QString::fromStdString(rhs.identity.sourceRecordId)
        );
        QVERIFY(sameIdentifiers(lhs.identity.externalIdentifiers, rhs.identity.externalIdentifiers));
        QCOMPARE(lhs.identity.aliases, rhs.identity.aliases);
        QVERIFY(sameCoordinate(lhs.fixedEquatorialValue(), rhs.fixedEquatorialValue()));
        QVERIFY(sameAstrometry(lhs.starAstrometryValue(), rhs.starAstrometryValue()));
        QVERIFY(sameDeepSkyInfo(lhs.deepSkyObjectValue(), rhs.deepSkyObjectValue()));
    }
}

void CatalogInterchangeabilityTests::malformedInputsFailConsistently()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    constexpr std::string_view kValidHeader = "obj_key\tcategory\tright_ascension_hours\tdeclination_deg\tvisual_mag\n";

    const CatalogPayloadParser parser;

    // Missing required column must be reported without constructing a catalog.
    QTest::ignoreMessage(
        QtWarningMsg, "Semantic Test TSV parse failed: Semantic test payload is missing a required column."
    );
    const auto missingColumn = CatalogLoader::load(
        CatalogSemanticFixtureAdapter::schemaType(),
        "obj_key\tcategory\tright_ascension_hours\tdeclination_deg\n"
        "sirius\tstar\t6.7525\t-16.7161\n"
    );
    QVERIFY(!missingColumn.isSuccess());
    QCOMPARE(missingColumn.errorCode, CatalogLoadResult::ErrorCode::MissingRequiredColumns);
    QVERIFY(missingColumn.catalog == nullptr);

    // Invalid numeric magnitude must fail before any catalog is constructed.
    QTest::ignoreMessage(
        QtWarningMsg,
        "Semantic Test TSV skipped 1 rows with invalid semantic rows; samples: row 2 has invalid coordinates or "
        "magnitude"
    );
    QTest::ignoreMessage(
        QtWarningMsg, "Semantic Test TSV parse failed: Semantic test payload does not contain any valid rows."
    );
    const auto invalidMagnitude =
        parser.parseResult(std::string(kValidHeader) + "sirius\tstar\t6.7525\t-16.7161\tnot-a-number\n");
    QVERIFY(!invalidMagnitude.isSuccess());
    QCOMPARE(invalidMagnitude.errorCode, CatalogLoadResult::ErrorCode::NoBodies);
    QVERIFY(invalidMagnitude.catalog == nullptr);

    // Unknown category must fail the same way.
    QTest::ignoreMessage(
        QtWarningMsg,
        "Semantic Test TSV skipped 1 rows with invalid semantic rows; samples: row 2 has an invalid key or category"
    );
    QTest::ignoreMessage(
        QtWarningMsg, "Semantic Test TSV parse failed: Semantic test payload does not contain any valid rows."
    );
    const auto unknownCategory =
        parser.parseResult(std::string(kValidHeader) + "sirius\tquasar\t6.7525\t-16.7161\t-1.46\n");
    QVERIFY(!unknownCategory.isSuccess());
    QCOMPARE(unknownCategory.errorCode, CatalogLoadResult::ErrorCode::NoBodies);
    QVERIFY(unknownCategory.catalog == nullptr);

    // A corrupted cache payload must be rejected before unsafe construction.
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());
    const QByteArray validPayload = CatalogBinaryCodec::serialize(semantic.catalog->catalog());
    QVERIFY(!validPayload.isEmpty());

    QByteArray corruptMagic = validPayload;
    corruptMagic[0] = static_cast<char>(0x00);
    QVERIFY(CatalogBinaryCodec::deserialize(corruptMagic) == nullptr);

    QByteArray truncated = validPayload.left(validPayload.size() / 2);
    QVERIFY(CatalogBinaryCodec::deserialize(truncated) == nullptr);
}

void CatalogInterchangeabilityTests::largeFixtureIndexesAndResolvesReferences()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    constexpr int kStarCount = 1500;

    std::string hygPayload = "id,hip,ra,dec,mag\n";
    std::string semanticPayload = "obj_key\tcategory\tright_ascension_hours\tdeclination_deg\tvisual_mag\thip_id\n";
    hygPayload.reserve(static_cast<std::size_t>(kStarCount) * 48U);
    semanticPayload.reserve(static_cast<std::size_t>(kStarCount) * 64U);
    for (int index = 0; index < kStarCount; ++index) {
        const int hip = 100000 + index;
        const double rightAscensionHours = std::fmod(index * 0.017, 24.0);
        const double declinationDeg = -60.0 + static_cast<double>(index % 120);
        const double magnitude = 3.0 + static_cast<double>(index % 50) / 10.0;
        hygPayload += std::to_string(index + 1);
        hygPayload += ',';
        hygPayload += std::to_string(hip);
        hygPayload += ',';
        hygPayload += std::to_string(rightAscensionHours);
        hygPayload += ',';
        hygPayload += std::to_string(declinationDeg);
        hygPayload += ',';
        hygPayload += std::to_string(magnitude);
        hygPayload += '\n';

        semanticPayload += std::to_string(index);
        semanticPayload += "\tstar\t";
        semanticPayload += std::to_string(rightAscensionHours);
        semanticPayload += '\t';
        semanticPayload += std::to_string(declinationDeg);
        semanticPayload += '\t';
        semanticPayload += std::to_string(magnitude);
        semanticPayload += '\t';
        semanticPayload += std::to_string(hip);
        semanticPayload += '\n';
    }

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(hygPayload);
    const auto semantic = parser.parseResult(semanticPayload);
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    const auto compose = [&](const bool semanticLast) {
        CatalogCompositionRequest request;
        if (semanticLast) {
            request.sources = {
                {.sourceId = "hyg",
                 .enabled = true,
                 .catalog = hyg.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "semantic",
                 .enabled = true,
                 .catalog = semantic.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
            };
        } else {
            request.sources = {
                {.sourceId = "semantic",
                 .enabled = true,
                 .catalog = semantic.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
                {.sourceId = "hyg",
                 .enabled = true,
                 .catalog = hyg.catalog.get(),
                 .policy = CatalogCompositionPolicy::Merge},
            };
        }
        return CatalogComposer::composeCollection(request);
    };

    const CatalogCompositionResult semanticLast = compose(true);
    QVERIFY(semanticLast.isSuccess());
    QCOMPARE(semanticLast.bodyCount, std::size_t{static_cast<std::size_t>(kStarCount)});
    QCOMPARE(semanticLast.starCount, std::size_t{static_cast<std::size_t>(kStarCount)});

    // Reference resolution through the shared identity index returns the
    // surviving canonical ID even though the semantic schema renamed the star.
    {
        const std::span<const BaseCelestialBody* const> bodies = semanticLast.catalog->bodies();
        ConstellationReferenceResolver resolver(bodies);
        const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_100042");
        QCOMPARE(
            static_cast<int>(resolution.status), static_cast<int>(ConstellationReferenceResolver::Status::Resolved)
        );
        QCOMPARE(QString::fromStdString(resolution.bodyId), QStringLiteral("star_42"));
    }

    const CatalogCompositionResult hygLast = compose(false);
    QVERIFY(hygLast.isSuccess());
    QCOMPARE(hygLast.bodyCount, std::size_t{static_cast<std::size_t>(kStarCount)});
    {
        const std::span<const BaseCelestialBody* const> bodies = hygLast.catalog->bodies();
        ConstellationReferenceResolver resolver(bodies);
        const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_100042");
        QCOMPARE(
            static_cast<int>(resolution.status), static_cast<int>(ConstellationReferenceResolver::Status::Resolved)
        );
        QCOMPARE(QString::fromStdString(resolution.bodyId), QStringLiteral("hip_100042"));
    }
}

QTEST_APPLESS_MAIN(CatalogInterchangeabilityTests)

#include "CatalogInterchangeabilityTests.moc"
