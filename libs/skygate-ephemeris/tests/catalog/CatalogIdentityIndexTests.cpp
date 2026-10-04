#include "DeepSkyObjectInfo.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogIdentityIndex.hpp"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::CatalogIdentityIndex;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

OwnGalaxyCelestialBody makeStar(std::string id, std::vector<CatalogIdentifier> identifiers = {})
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.kind = BaseCelestialBody::Kind::Star;
    body.identity.externalIdentifiers = std::move(identifiers);
    return body;
}

DistantCelestialBody makeDeepSkyObject(
    std::string id, std::vector<std::string> aliases = {}, std::vector<CatalogIdentifier> identifiers = {}
)
{
    DistantCelestialBody body;
    body.id = std::move(id);
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.identity.aliases = aliases;
    body.identity.externalIdentifiers = std::move(identifiers);
    body.deepSkyObject = DeepSkyObjectInfo{.kind = DeepSkyObjectInfo::Kind::Galaxy, .aliases = std::move(aliases)};
    return body;
}

}  // namespace

class CatalogIdentityIndexTests final : public QObject {
    Q_OBJECT

private slots:
    void resolvesCanonicalIdCaseInsensitively();
    void resolvesExternalIdentifierAcrossCanonicalIds();
    void doesNotCollideAcrossNamespaces();
    void resolvesDeepSkyAlias();
    void doesNotMatchStarsByDisplayName();
    void reportsAmbiguousAuthoritativeMatch();
    void reportsAmbiguousAliasMatch();
    void resolvesIdentifierChainsIncrementally();
    void ignoresBodiesWithoutIdentity();
};

void CatalogIdentityIndexTests::resolvesCanonicalIdCaseInsensitively()
{
    const OwnGalaxyCelestialBody registered = makeStar("hip_123");
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    const OwnGalaxyCelestialBody incoming = makeStar("HIP_123");
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QVERIFY(resolution.hasSingleMatch());
    QVERIFY(resolution.isAuthoritative());
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::CanonicalId);
    QCOMPARE(resolution.index, std::size_t{0});
}

void CatalogIdentityIndexTests::resolvesExternalIdentifierAcrossCanonicalIds()
{
    const OwnGalaxyCelestialBody registered = makeStar("catalog_a_1", {CatalogIdentifier::make("hip", "32349")});
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    const OwnGalaxyCelestialBody incoming = makeStar("catalog_b_9", {CatalogIdentifier::make("hip", "032349")});
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QVERIFY(resolution.hasSingleMatch());
    QVERIFY(resolution.isAuthoritative());
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::ExternalIdentifier);
    QCOMPARE(resolution.index, std::size_t{0});
}

void CatalogIdentityIndexTests::doesNotCollideAcrossNamespaces()
{
    const OwnGalaxyCelestialBody registered = makeStar("hip_123", {CatalogIdentifier::make("hip", "123")});
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    const OwnGalaxyCelestialBody incoming = makeStar("hyg_123", {CatalogIdentifier::make("hyg", "123")});
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::None);
    QVERIFY(!resolution.hasSingleMatch());
    QVERIFY(!resolution.isAmbiguous());
}

void CatalogIdentityIndexTests::resolvesDeepSkyAlias()
{
    const DistantCelestialBody registered = makeDeepSkyObject("ngc_224", {"M31"});
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    const DistantCelestialBody incoming = makeDeepSkyObject("open_ngc_m31", {"M 31"});
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QVERIFY(resolution.hasSingleMatch());
    QVERIFY(!resolution.isAuthoritative());
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::Alias);
    QCOMPARE(resolution.index, std::size_t{0});
}

void CatalogIdentityIndexTests::doesNotMatchStarsByDisplayName()
{
    OwnGalaxyCelestialBody registered = makeStar("hip_1");
    registered.displayName = "Sirius";
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    OwnGalaxyCelestialBody incoming = makeStar("hip_2");
    incoming.displayName = "Sirius";
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::None);
}

void CatalogIdentityIndexTests::reportsAmbiguousAuthoritativeMatch()
{
    const OwnGalaxyCelestialBody star = makeStar("hip_1", {CatalogIdentifier::make("hip", "123")});
    const DistantCelestialBody deepSkyObject =
        makeDeepSkyObject("ngc_123", {}, {CatalogIdentifier::make("hip", "123")});
    CatalogIdentityIndex index;
    index.add(star, 0U);
    index.add(deepSkyObject, 1U);

    const OwnGalaxyCelestialBody incoming = makeStar("hip_9", {CatalogIdentifier::make("hip", "123")});
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QVERIFY(resolution.isAmbiguous());
    QVERIFY(resolution.isAuthoritative());
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::AmbiguousAuthoritative);
    QCOMPARE(resolution.candidates.size(), std::size_t{2});
    QCOMPARE(resolution.candidates[0], std::size_t{0});
    QCOMPARE(resolution.candidates[1], std::size_t{1});
}

void CatalogIdentityIndexTests::reportsAmbiguousAliasMatch()
{
    const DistantCelestialBody first = makeDeepSkyObject("ngc_224", {"M31"});
    const DistantCelestialBody second = makeDeepSkyObject("ngc_598", {"M31"});
    CatalogIdentityIndex index;
    index.add(first, 0U);
    index.add(second, 1U);

    const DistantCelestialBody incoming = makeDeepSkyObject("open_ngc_unknown", {"M 31"});
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);

    QVERIFY(resolution.isAmbiguous());
    QVERIFY(!resolution.isAuthoritative());
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::AmbiguousAlias);
    QCOMPARE(resolution.candidates.size(), std::size_t{2});
}

void CatalogIdentityIndexTests::resolvesIdentifierChainsIncrementally()
{
    const OwnGalaxyCelestialBody first = makeStar("cat_a_1", {CatalogIdentifier::make("hip", "1")});
    CatalogIdentityIndex index;
    index.add(first, 0U);

    const OwnGalaxyCelestialBody second =
        makeStar("cat_a_2", {CatalogIdentifier::make("hip", "1"), CatalogIdentifier::make("hyg", "5")});
    const CatalogIdentityIndex::Resolution secondResolution = index.resolve(second);
    QVERIFY(secondResolution.hasSingleMatch());
    QCOMPARE(secondResolution.index, std::size_t{0});
    index.add(second, secondResolution.index);

    const OwnGalaxyCelestialBody third = makeStar("cat_a_3", {CatalogIdentifier::make("hyg", "5")});
    const CatalogIdentityIndex::Resolution thirdResolution = index.resolve(third);
    QVERIFY(thirdResolution.hasSingleMatch());
    QVERIFY(thirdResolution.isAuthoritative());
    QCOMPARE(thirdResolution.index, std::size_t{0});
}

void CatalogIdentityIndexTests::ignoresBodiesWithoutIdentity()
{
    const OwnGalaxyCelestialBody registered = makeStar("hip_1");
    CatalogIdentityIndex index;
    index.add(registered, 0U);

    const OwnGalaxyCelestialBody incoming = makeStar("hip_2");
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);
    QCOMPARE(resolution.kind, CatalogIdentityIndex::Resolution::MatchKind::None);
}

QTEST_APPLESS_MAIN(CatalogIdentityIndexTests)

#include "CatalogIdentityIndexTests.moc"
