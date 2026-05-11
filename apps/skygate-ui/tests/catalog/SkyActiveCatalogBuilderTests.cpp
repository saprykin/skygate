#include "catalog/SkyActiveCatalogBuilder.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <string>
#include <utility>

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

}  // namespace

class SkyActiveCatalogBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void assignsSourceIdsAndFallbackLabels();
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

QTEST_APPLESS_MAIN(SkyActiveCatalogBuilderTests)

#include "SkyActiveCatalogBuilderTests.moc"
