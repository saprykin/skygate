#include "DistantCelestialBody.hpp"
#include "catalog/CatalogBodyParseResult.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "catalog/ICatalogParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

namespace {

[[nodiscard]] std::unique_ptr<skygate::ephemeris::ICatalogParser> bundledParser()
{
    return skygate::ephemeris::CatalogSchemaRegistry::createParser(skygate::ephemeris::CatalogSourceType::Bundled);
}

[[nodiscard]] const skygate::ephemeris::DistantCelestialBody*
findDistantBody(const skygate::ephemeris::CatalogBodyParseResult& result, const std::string& id)
{
    const auto body = std::find_if(
        result.distantBodies.begin(),
        result.distantBodies.end(),
        [&id](const skygate::ephemeris::DistantCelestialBody& candidate) { return candidate.id == id; }
    );
    return body == result.distantBodies.end() ? nullptr : &*body;
}

}  // namespace

class BundledCatalogParserTests final : public QObject {
    Q_OBJECT

private slots:
    void reportsEveryBundledRow();
    void keepsZeroPositionAngleAndDropsZeroExtent();
};

void BundledCatalogParserTests::reportsEveryBundledRow()
{
    const std::unique_ptr<skygate::ephemeris::ICatalogParser> parser = bundledParser();
    QVERIFY(parser != nullptr);

    const skygate::ephemeris::CatalogBodyParseResult result = parser->parse({}, {});
    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodies.size(), std::size_t{9});
    QCOMPARE(result.distantBodies.size(), std::size_t{110});
    QCOMPARE(result.orderedBodyIndexes.size(), std::size_t{119});
    // Both diagnostics count the bodies of both storage domains, matching the
    // total the loader derives from the parsed vectors.
    QCOMPARE(result.diagnostics.parsedBodyCount, std::size_t{119});
    QCOMPARE(result.diagnostics.processedRowCount, std::size_t{119});
}

void BundledCatalogParserTests::keepsZeroPositionAngleAndDropsZeroExtent()
{
    const std::unique_ptr<skygate::ephemeris::ICatalogParser> parser = bundledParser();
    QVERIFY(parser != nullptr);

    const skygate::ephemeris::CatalogBodyParseResult result = parser->parse({}, {});
    QVERIFY(result.isSuccess());

    const skygate::ephemeris::DistantCelestialBody* crabNebula = findDistantBody(result, "messier_001");
    QVERIFY(crabNebula != nullptr);
    QVERIFY(crabNebula->deepSkyObject.has_value());
    QVERIFY(crabNebula->deepSkyObject->majorAxisArcmin.has_value());
    QCOMPARE(*crabNebula->deepSkyObject->majorAxisArcmin, 8.0);
    QVERIFY(crabNebula->deepSkyObject->positionAngleDeg.has_value());
    QCOMPARE(*crabNebula->deepSkyObject->positionAngleDeg, 0.0);

    const skygate::ephemeris::DistantCelestialBody* globularCluster = findDistantBody(result, "messier_002");
    QVERIFY(globularCluster != nullptr);
    QVERIFY(globularCluster->deepSkyObject.has_value());
    QVERIFY(globularCluster->deepSkyObject->majorAxisArcmin.has_value());
    QVERIFY(!globularCluster->deepSkyObject->minorAxisArcmin.has_value());
    QVERIFY(globularCluster->deepSkyObject->positionAngleDeg.has_value());
    QCOMPARE(*globularCluster->deepSkyObject->positionAngleDeg, 0.0);
}

QTEST_APPLESS_MAIN(BundledCatalogParserTests)

#include "BundledCatalogParserTests.moc"
