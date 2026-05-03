#include "skygate/ephemeris/CatalogComposer.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const skygate::ephemeris::CelestialBodyEphemerisSource source
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.ephemerisSource = source;
    if (source == skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate {
            .rightAscensionHours = 1.0,
            .declinationDeg = 2.0
        };
    }
    return body;
}

skygate::ephemeris::CelestialBody makeDeepSkyObject(
    std::string id,
    std::string displayName,
    std::vector<std::string> aliases
)
{
    auto body = makeBody(
        std::move(id),
        std::move(displayName),
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
    );
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = std::move(aliases)
    };
    return body;
}

std::optional<std::size_t> bodyIndexById(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::string& id
)
{
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace

class CatalogComposerTests final : public QObject {
    Q_OBJECT

private slots:
    void tagsPrimaryAndBuiltInSources();
    void replacesPrimaryDeepSkyAliasWithDownloadedObject();
    void bundledFallbackAddsDeepSkySourceKinds();
    void bundledFallbackCanBeDisabled();
    void preservesKnownDeepSkyObjectCountWhenProvided();
};

void CatalogComposerTests::tagsPrimaryAndBuiltInSources()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "sun",
            "Sun",
            skygate::ephemeris::CelestialBodyType::Sun,
            skygate::ephemeris::CelestialBodyEphemerisSource::Sun
        ),
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::CelestialBodyType::Star,
            skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *sourceCatalog
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.sourceKinds.size(), result.catalog->bodies().size());

    const auto sunIndex = bodyIndexById(result.catalog->bodies(), "sun");
    const auto starIndex = bodyIndexById(result.catalog->bodies(), "hip_1");
    QVERIFY(sunIndex.has_value());
    QVERIFY(starIndex.has_value());
    QCOMPARE(
        result.sourceKinds[*sunIndex],
        skygate::ephemeris::CatalogCompositionSource::BuiltInEphemeris
    );
    QCOMPARE(result.sourceKinds[*starIndex], skygate::ephemeris::CatalogCompositionSource::Primary);
}

void CatalogComposerTests::replacesPrimaryDeepSkyAliasWithDownloadedObject()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::CelestialBodyType::Star,
            skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
        ),
        makeDeepSkyObject("messier_031", "M31", {"M31", "NGC 224"}),
    });
    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyObject("open_ngc_m31", "OpenNGC M31", {"M 31", "NGC0224"}),
    });
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *sourceCatalog,
        .deepSkyCatalog = deepSkyCatalog.get()
    });

    QVERIFY(result.isSuccess());
    QVERIFY(!bodyIndexById(result.catalog->bodies(), "messier_031").has_value());
    const auto downloadedIndex = bodyIndexById(result.catalog->bodies(), "open_ngc_m31");
    QVERIFY(downloadedIndex.has_value());
    QCOMPARE(
        result.sourceKinds[*downloadedIndex],
        skygate::ephemeris::CatalogCompositionSource::DeepSky
    );
    QCOMPARE(result.foundDeepSkyObjectCount, 1U);
}

void CatalogComposerTests::bundledFallbackAddsDeepSkySourceKinds()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::CelestialBodyType::Star,
            skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *sourceCatalog,
        .useBundledDeepSkyCatalog = true
    });

    QVERIFY(result.isSuccess());
    QVERIFY(result.deepSkyObjectCount > 0U);
    QVERIFY(result.foundDeepSkyObjectCount > 0U);
    QVERIFY(std::any_of(
        result.sourceKinds.begin(),
        result.sourceKinds.end(),
        [](const skygate::ephemeris::CatalogCompositionSource source) {
            return source == skygate::ephemeris::CatalogCompositionSource::DeepSky;
        }
    ));
}

void CatalogComposerTests::bundledFallbackCanBeDisabled()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::CelestialBodyType::Star,
            skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
        ),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *sourceCatalog,
        .useBundledDeepSkyCatalog = false
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, 0U);
    QCOMPARE(result.foundDeepSkyObjectCount, 0U);
    QVERIFY(std::none_of(
        result.sourceKinds.begin(),
        result.sourceKinds.end(),
        [](const skygate::ephemeris::CatalogCompositionSource source) {
            return source == skygate::ephemeris::CatalogCompositionSource::DeepSky;
        }
    ));
}

void CatalogComposerTests::preservesKnownDeepSkyObjectCountWhenProvided()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_1",
            "HIP 1",
            skygate::ephemeris::CelestialBodyType::Star,
            skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
        ),
    });
    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"}),
    });
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *sourceCatalog,
        .deepSkyCatalog = deepSkyCatalog.get(),
        .knownDeepSkyObjectCount = 42U
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.foundDeepSkyObjectCount, 42U);
    QVERIFY(result.deepSkyObjectCount >= 1U);
}

QTEST_APPLESS_MAIN(CatalogComposerTests)

#include "CatalogComposerTests.moc"
