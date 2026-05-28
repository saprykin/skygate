#include "catalog/CatalogComposer.hpp"
#include "catalog/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::OwnGalaxyCelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const skygate::ephemeris::BaseCelestialBody::Kind source
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = type;
    if (source == skygate::ephemeris::BaseCelestialBody::Kind::Star) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    }
    return body;
}

skygate::ephemeris::DistantCelestialBody
makeDeepSkyObject(std::string id, std::string displayName, std::vector<std::string> aliases)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
        .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy, .aliases = std::move(aliases)
    };
    return body;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog> createCatalog(
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
bodyIndexById(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const std::string& id)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            return index;
        }
    }
    return std::nullopt;
}

std::size_t
countBodiesById(const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const std::string& id)
{
    return static_cast<std::size_t>(
        std::count_if(bodies.begin(), bodies.end(), [&id](const skygate::ephemeris::BaseCelestialBody* body) {
            return body != nullptr && body->id == id;
        })
    );
}

}  // namespace

class CatalogComposerTests final : public QObject {
    Q_OBJECT

private slots:
    void tagsPrimaryAndBuiltInSources();
    void replacesPrimaryDeepSkyAliasWithDownloadedObject();
    void replacesDeepSkyObjectsByNormalizedPrimaryIdentity();
    void bundledFallbackAddsDeepSkySourceKinds();
    void bundledFallbackCanBeDisabled();
    void doesNotDuplicatePrimarySolarSystemBodies();
    void addsBundledBrightStarsWhenSourceHasNoStars();
    void doesNotAddBundledBrightStarsWhenSourceHasStars();
    void usesCurrentConstellationCountWhenLarger();
    void ignoresNonDeepSkyRowsFromDeepSkyCatalog();
    void preservesKnownDeepSkyObjectCountWhenProvided();
};

void CatalogComposerTests::tagsPrimaryAndBuiltInSources()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "sun",
            "Sun",
            skygate::ephemeris::BaseCelestialBody::Kind::Sun,
            skygate::ephemeris::BaseCelestialBody::Kind::Sun
        ),
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose({.sourceCatalog = *sourceCatalog});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.sourceKinds.size(), result.catalog->bodies().size());

    const auto sunIndex = bodyIndexById(result.catalog->bodies(), "sun");
    const auto starIndex = bodyIndexById(result.catalog->bodies(), "hip_1");
    QVERIFY(sunIndex.has_value());
    QVERIFY(starIndex.has_value());
    QCOMPARE(result.sourceKinds[*sunIndex], skygate::ephemeris::CatalogCompositionSource::BuiltInEphemeris);
    QCOMPARE(result.sourceKinds[*starIndex], skygate::ephemeris::CatalogCompositionSource::Primary);
}

void CatalogComposerTests::replacesPrimaryDeepSkyAliasWithDownloadedObject()
{
    auto sourceCatalog = createCatalog(
        {makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        )},
        {makeDeepSkyObject("messier_031", "M31", {"M31", "NGC 224"})}
    );
    auto deepSkyCatalog = createCatalog({}, {makeDeepSkyObject("open_ngc_m31", "OpenNGC M31", {"M 31", "NGC0224"})});
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .deepSkyCatalog = deepSkyCatalog.get()}
    );

    QVERIFY(result.isSuccess());
    QVERIFY(!bodyIndexById(result.catalog->bodies(), "messier_031").has_value());
    const auto downloadedIndex = bodyIndexById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(downloadedIndex.has_value());
    QCOMPARE(result.sourceKinds[*downloadedIndex], skygate::ephemeris::CatalogCompositionSource::DeepSky);
    QCOMPARE(result.foundDeepSkyObjectCount, 1U);
}

void CatalogComposerTests::replacesDeepSkyObjectsByNormalizedPrimaryIdentity()
{
    auto sourceCatalog = createCatalog(
        {makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        )},
        {makeDeepSkyObject("ngc_224", "Primary M31", {})}
    );
    auto deepSkyCatalog = createCatalog({}, {makeDeepSkyObject("open_ngc_m31", "OpenNGC M31", {"NGC 224"})});
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .deepSkyCatalog = deepSkyCatalog.get()}
    );

    QVERIFY(result.isSuccess());
    QVERIFY(!bodyIndexById(result.catalog->bodies(), "ngc_224").has_value());
    QVERIFY(bodyIndexById(result.catalog->bodies(), "open_ngc_m31").has_value());
}

void CatalogComposerTests::bundledFallbackAddsDeepSkySourceKinds()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .useBundledDeepSkyCatalog = true}
    );

    QVERIFY(result.isSuccess());
    QVERIFY(result.deepSkyObjectCount > 0U);
    QVERIFY(result.foundDeepSkyObjectCount > 0U);
    QVERIFY(
        std::any_of(
            result.sourceKinds.begin(),
            result.sourceKinds.end(),
            [](const skygate::ephemeris::CatalogCompositionSource source) {
                return source == skygate::ephemeris::CatalogCompositionSource::DeepSky;
            }
        )
    );
}

void CatalogComposerTests::doesNotDuplicatePrimarySolarSystemBodies()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "sun",
            "Sun",
            skygate::ephemeris::BaseCelestialBody::Kind::Sun,
            skygate::ephemeris::BaseCelestialBody::Kind::Sun
        ),
        makeBody(
            "moon",
            "Moon",
            skygate::ephemeris::BaseCelestialBody::Kind::Moon,
            skygate::ephemeris::BaseCelestialBody::Kind::Moon
        ),
        makeBody(
            "mars",
            "Mars",
            skygate::ephemeris::BaseCelestialBody::Kind::Planet,
            skygate::ephemeris::BaseCelestialBody::Kind::Planet
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose({.sourceCatalog = *sourceCatalog});

    QVERIFY(result.isSuccess());
    QCOMPARE(countBodiesById(result.catalog->bodies(), "sun"), 1U);
    QCOMPARE(countBodiesById(result.catalog->bodies(), "moon"), 1U);
    QCOMPARE(countBodiesById(result.catalog->bodies(), "mars"), 1U);
}

void CatalogComposerTests::addsBundledBrightStarsWhenSourceHasNoStars()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "orion",
            "Orion",
            skygate::ephemeris::BaseCelestialBody::Kind::Constellation,
            skygate::ephemeris::BaseCelestialBody::Kind::Constellation
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose({.sourceCatalog = *sourceCatalog});

    QVERIFY(result.isSuccess());
    const auto siriusIndex = bodyIndexById(result.catalog->bodies(), "sirius");
    QVERIFY(siriusIndex.has_value());
    QCOMPARE(result.sourceKinds[*siriusIndex], skygate::ephemeris::CatalogCompositionSource::BuiltInEphemeris);
}

void CatalogComposerTests::doesNotAddBundledBrightStarsWhenSourceHasStars()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose({.sourceCatalog = *sourceCatalog});

    QVERIFY(result.isSuccess());
    QVERIFY(!bodyIndexById(result.catalog->bodies(), "sirius").has_value());
}

void CatalogComposerTests::usesCurrentConstellationCountWhenLarger()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .currentConstellationCount = 12U}
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.constellationCount, 12U);
}

void CatalogComposerTests::ignoresNonDeepSkyRowsFromDeepSkyCatalog()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    auto deepSkyCatalog = createCatalog(
        {makeBody(
            "hip_bad",
            "Not Deep Sky",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        )},
        {makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"})}
    );
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .deepSkyCatalog = deepSkyCatalog.get()}
    );

    QVERIFY(result.isSuccess());
    QVERIFY(!bodyIndexById(result.catalog->bodies(), "hip_bad").has_value());
    QVERIFY(bodyIndexById(result.catalog->bodies(), "ngc_1").has_value());
}

void CatalogComposerTests::bundledFallbackCanBeDisabled()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .useBundledDeepSkyCatalog = false}
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, 0U);
    QCOMPARE(result.foundDeepSkyObjectCount, 0U);
    QVERIFY(
        std::none_of(
            result.sourceKinds.begin(),
            result.sourceKinds.end(),
            [](const skygate::ephemeris::CatalogCompositionSource source) {
                return source == skygate::ephemeris::CatalogCompositionSource::DeepSky;
            }
        )
    );
}

void CatalogComposerTests::preservesKnownDeepSkyObjectCountWhenProvided()
{
    auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::BaseCelestialBody::Kind::Star,
            skygate::ephemeris::BaseCelestialBody::Kind::Star
        ),
    });
    auto deepSkyCatalog = createCatalog({}, {makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"})});
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::CatalogComposer::compose(
        {.sourceCatalog = *sourceCatalog, .deepSkyCatalog = deepSkyCatalog.get(), .knownDeepSkyObjectCount = 42U}
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.foundDeepSkyObjectCount, 42U);
    QVERIFY(result.deepSkyObjectCount >= 1U);
}

QTEST_APPLESS_MAIN(CatalogComposerTests)

#include "CatalogComposerTests.moc"
