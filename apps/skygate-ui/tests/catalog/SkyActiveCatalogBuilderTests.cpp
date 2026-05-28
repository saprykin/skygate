#include "catalog/CatalogFactory.hpp"
#include "catalog/SkyActiveCatalogBuilder.hpp"

#include <QtTest/QtTest>

#include <string>
#include <utility>

namespace {

skygate::ephemeris::OwnGalaxyCelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const skygate::ephemeris::BaseCelestialBody::Kind source,
    const double magnitude = 1.0
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = type;
    body.visualMagnitude = magnitude;
    if (source == skygate::ephemeris::BaseCelestialBody::Kind::Star) {
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.0, .declinationDeg = 2.0};
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

    const auto result = skygate::ui::internal::SkyActiveCatalogBuilder::build(
        {.sourceCatalog = *sourceCatalog,
         .useBundledDeepSkyCatalog = false,
         .sourceLabel = " ",
         .deepSkySourceLabel = " "}
    );

    QVERIFY(result.isSuccess());
    QCOMPARE(result.sourceLabels, QStringList({"Catalog", "Deep sky catalog", "Built-in ephemeris"}));
    QCOMPARE(result.sourceIds.size(), result.bodyCount);
    QCOMPARE(result.sourceIds[0], 2U);
    QCOMPARE(result.sourceIds[1], 0U);
}

QTEST_APPLESS_MAIN(SkyActiveCatalogBuilderTests)

#include "SkyActiveCatalogBuilderTests.moc"
