#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/constellation/ConstellationReferenceResolver.hpp"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::ConstellationAnchorGroup;
using skygate::ephemeris::ConstellationLineRef;
using skygate::ephemeris::ConstellationReferenceResolver;
using skygate::ephemeris::OwnGalaxyCelestialBody;

OwnGalaxyCelestialBody makeStar(std::string id, std::vector<CatalogIdentifier> identifiers = {})
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.kind = BaseCelestialBody::Kind::Star;
    body.identity.externalIdentifiers = std::move(identifiers);
    return body;
}

struct ResolverFixture final {
    std::vector<OwnGalaxyCelestialBody> storage;
    std::vector<const BaseCelestialBody*> bodies;

    void add(OwnGalaxyCelestialBody body)
    {
        storage.push_back(std::move(body));
        bodies.clear();
        bodies.reserve(storage.size());
        for (const OwnGalaxyCelestialBody& stored : storage) {
            bodies.push_back(&stored);
        }
    }

    [[nodiscard]] ConstellationReferenceResolver resolver() const
    {
        return ConstellationReferenceResolver(bodies);
    }
};

}  // namespace

class ConstellationReferenceResolverTests final : public QObject {
    Q_OBJECT

private slots:
    void resolvesHipReferenceToDifferentlyNamedCanonicalId();
    void resolvesCanonicalHipIdForHygStyleBody();
    void reportsUnresolvedReference();
    void reportsAmbiguousReference();
    void doesNotCollideAcrossNamespaces();
    void treatsNonHipReferenceAsUnresolved();
    void resolveLinesKeepsOnlyFullyResolvedSegments();
    void resolveAnchorsKeepsResolvedAnchorsAndDropsEmptyGroups();
};

void ConstellationReferenceResolverTests::resolvesHipReferenceToDifferentlyNamedCanonicalId()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_a_27989", {CatalogIdentifier::make("hip", "27989")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_27989");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Resolved);
    QVERIFY(resolution.bodyId == "catalog_a_27989");
}

void ConstellationReferenceResolverTests::resolvesCanonicalHipIdForHygStyleBody()
{
    ResolverFixture fixture;
    fixture.add(makeStar("hip_27989", {CatalogIdentifier::make("hip", "27989")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_27989");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Resolved);
    QVERIFY(resolution.bodyId == "hip_27989");
}

void ConstellationReferenceResolverTests::reportsUnresolvedReference()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_a_1", {CatalogIdentifier::make("hip", "1")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_42");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Unresolved);
    QVERIFY(resolution.bodyId.empty());
}

void ConstellationReferenceResolverTests::reportsAmbiguousReference()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_a_1", {CatalogIdentifier::make("hip", "123")}));
    fixture.add(makeStar("catalog_b_9", {CatalogIdentifier::make("hip", "123")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_123");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Ambiguous);
    QVERIFY(resolution.bodyId.empty());
}

void ConstellationReferenceResolverTests::doesNotCollideAcrossNamespaces()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_hyg_123", {CatalogIdentifier::make("hyg", "123")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("hip_123");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Unresolved);
}

void ConstellationReferenceResolverTests::treatsNonHipReferenceAsUnresolved()
{
    ResolverFixture fixture;
    fixture.add(makeStar("ngc_224"));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const ConstellationReferenceResolver::Resolution resolution = resolver.resolve("ngc_224");

    QCOMPARE(resolution.status, ConstellationReferenceResolver::Status::Unresolved);
}

void ConstellationReferenceResolverTests::resolveLinesKeepsOnlyFullyResolvedSegments()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_a_1", {CatalogIdentifier::make("hip", "1")}));
    fixture.add(makeStar("catalog_a_2", {CatalogIdentifier::make("hip", "2")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const std::vector<ConstellationLineRef> lineRefs{
        {"hip_1", "hip_2"},
        {"hip_2", "hip_999"},
    };

    const std::vector<ConstellationLineRef> resolved = resolver.resolveLines(lineRefs);

    QCOMPARE(resolved.size(), std::size_t{1});
    QVERIFY(resolved[0].first == "catalog_a_1");
    QVERIFY(resolved[0].second == "catalog_a_2");
}

void ConstellationReferenceResolverTests::resolveAnchorsKeepsResolvedAnchorsAndDropsEmptyGroups()
{
    ResolverFixture fixture;
    fixture.add(makeStar("catalog_a_1", {CatalogIdentifier::make("hip", "1")}));
    fixture.add(makeStar("catalog_a_2", {CatalogIdentifier::make("hip", "2")}));
    const ConstellationReferenceResolver resolver = fixture.resolver();

    const std::vector<ConstellationAnchorGroup> anchorGroups{
        {"Resolved", {"hip_1", "hip_2"}},
        {"Partial", {"hip_1", "hip_999"}},
        {"Missing", {"hip_999", "hip_1000"}},
    };

    const std::vector<ConstellationAnchorGroup> resolved = resolver.resolveAnchors(anchorGroups);

    QCOMPARE(resolved.size(), std::size_t{2});
    QVERIFY(resolved[0].first == "Resolved");
    QCOMPARE(resolved[0].second.size(), std::size_t{2});
    QVERIFY(resolved[0].second[0] == "catalog_a_1");
    QVERIFY(resolved[0].second[1] == "catalog_a_2");
    QVERIFY(resolved[1].first == "Partial");
    QCOMPARE(resolved[1].second.size(), std::size_t{1});
    QVERIFY(resolved[1].second[0] == "catalog_a_1");
}

QTEST_APPLESS_MAIN(ConstellationReferenceResolverTests)

#include "ConstellationReferenceResolverTests.moc"
