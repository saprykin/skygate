#include "catalog/SkyActiveCatalogBuilder.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const skygate::ephemeris::CelestialBodyEphemerisSource source,
    const double magnitude = 1.0
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.ephemerisSource = source;
    body.visualMagnitude = magnitude;
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

bool containsBody(
    const std::span<const skygate::ephemeris::CelestialBody> bodies,
    const std::string& id
)
{
    return std::any_of(
        bodies.begin(),
        bodies.end(),
        [&id](const skygate::ephemeris::CelestialBody& body) {
            return body.id == id;
        }
    );
}

}  // namespace

class SkyActiveCatalogBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void assignsSourceIdsAndFallbackLabels();
    void downloadedDeepSkyReplacesPrimaryAliasMatch();
    void deepSkyCatalogIgnoresNonDeepSkyBodies();
    void knownDeepSkyCountIsPreservedWhenProvided();
};

void SkyActiveCatalogBuilderTests::assignsSourceIdsAndFallbackLabels()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("sun", "Sun", skygate::ephemeris::CelestialBodyType::Sun, skygate::ephemeris::CelestialBodyEphemerisSource::Sun),
        makeBody("hip_1", "HIP 1", skygate::ephemeris::CelestialBodyType::Star, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial),
    });
    QVERIFY(sourceCatalog != nullptr);

    const auto result = skygate::ui::internal::SkyActiveCatalogBuilder::build({
        .sourceCatalog = *sourceCatalog,
        .useBundledDeepSkyCatalog = false,
        .sourceLabel = " ",
        .deepSkySourceLabel = " "
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.sourceLabels, QStringList({"Catalog", "Deep sky catalog", "Built-in ephemeris"}));
    QCOMPARE(result.sourceIds.size(), result.bodyCount);
    QCOMPARE(result.sourceIds[0], 2U);
    QCOMPARE(result.sourceIds[1], 0U);
}

void SkyActiveCatalogBuilderTests::downloadedDeepSkyReplacesPrimaryAliasMatch()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("hip_1", "HIP 1", skygate::ephemeris::CelestialBodyType::Star, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial),
        makeDeepSkyObject("messier_031", "M31", {"M31", "NGC 224"}),
    });
    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyObject("open_ngc_m31", "OpenNGC M31", {"M 31", "NGC0224"}),
    });
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    const auto result = skygate::ui::internal::SkyActiveCatalogBuilder::build({
        .sourceCatalog = *sourceCatalog,
        .deepSkyCatalog = deepSkyCatalog.get(),
        .sourceLabel = "Primary",
        .deepSkySourceLabel = "OpenNGC"
    });

    QVERIFY(result.isSuccess());
    const auto bodies = result.catalog->bodies();
    QVERIFY(containsBody(bodies, "hip_1"));
    QVERIFY(!containsBody(bodies, "messier_031"));
    QVERIFY(containsBody(bodies, "open_ngc_m31"));
    QVERIFY(result.deepSkyObjectCount >= 1U);
}

void SkyActiveCatalogBuilderTests::deepSkyCatalogIgnoresNonDeepSkyBodies()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("hip_1", "HIP 1", skygate::ephemeris::CelestialBodyType::Star, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial),
    });
    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("hip_2", "HIP 2", skygate::ephemeris::CelestialBodyType::Star, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial),
        makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"}),
    });
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    const auto result = skygate::ui::internal::SkyActiveCatalogBuilder::build({
        .sourceCatalog = *sourceCatalog,
        .deepSkyCatalog = deepSkyCatalog.get(),
        .sourceLabel = "Primary",
        .deepSkySourceLabel = "Deep"
    });

    QVERIFY(result.isSuccess());
    const auto bodies = result.catalog->bodies();
    QVERIFY(containsBody(bodies, "hip_1"));
    QVERIFY(!containsBody(bodies, "hip_2"));
    QVERIFY(containsBody(bodies, "ngc_1"));
}

void SkyActiveCatalogBuilderTests::knownDeepSkyCountIsPreservedWhenProvided()
{
    auto sourceCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody("hip_1", "HIP 1", skygate::ephemeris::CelestialBodyType::Star, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial),
    });
    auto deepSkyCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeDeepSkyObject("ngc_1", "NGC 1", {"NGC 1"}),
    });
    QVERIFY(sourceCatalog != nullptr);
    QVERIFY(deepSkyCatalog != nullptr);

    const auto result = skygate::ui::internal::SkyActiveCatalogBuilder::build({
        .sourceCatalog = *sourceCatalog,
        .deepSkyCatalog = deepSkyCatalog.get(),
        .knownDeepSkyObjectCount = 42,
        .sourceLabel = "Primary",
        .deepSkySourceLabel = "Deep"
    });

    QVERIFY(result.isSuccess());
    QCOMPARE(result.foundDeepSkyObjectCount, 42U);
}

QTEST_APPLESS_MAIN(SkyActiveCatalogBuilderTests)

#include "SkyActiveCatalogBuilderTests.moc"
