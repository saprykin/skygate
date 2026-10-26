#include "catalog/CatalogSchemaRegistry.hpp"
#include "catalog/ICatalogParser.hpp"
#include "catalog/io/CatalogPayloadFormatDetector.hpp"

#include <QtTest/QtTest>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

class TestCatalogParser final : public skygate::ephemeris::ICatalogParser {
public:
    skygate::ephemeris::CatalogBodyParseResult parse(
        std::string_view data, const skygate::ephemeris::CatalogParseProgressCallback& progressCallback
    ) const override
    {
        Q_UNUSED(data);
        Q_UNUSED(progressCallback);

        skygate::ephemeris::CatalogBodyParseResult result;
        skygate::ephemeris::OwnGalaxyCelestialBody body;
        body.id = "test_1";
        body.displayName = "Test Body";
        body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
        body.visualMagnitude = 1.0;
        result.orderedBodyIndexes.push_back(
            skygate::ephemeris::CelestialBodyCatalog::OrderEntry{
                .domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = 0U,
            }
        );
        result.bodies.push_back(std::move(body));
        result.diagnostics.parsedBodyCount = 1U;
        return result;
    }
};

constexpr auto kTestSchemaType = static_cast<skygate::ephemeris::CatalogSourceType>(0xF0);

}  // namespace

class CatalogSchemaRegistryTests final : public QObject {
    Q_OBJECT

private slots:
    void registersBuiltInSchemas();
    void registeredSchemaDrivesDetectionAndSelection();
    void reportsUnknownSchemas();
};

void CatalogSchemaRegistryTests::registersBuiltInSchemas()
{
    using namespace skygate::ephemeris;

    const CatalogSchemaDescriptor* hyg = CatalogSchemaRegistry::find(CatalogSourceType::HygCsv);
    QVERIFY(hyg != nullptr);
    QCOMPARE(hyg->diagnosticName, std::string("HYG CSV"));
    QVERIFY(hyg->delimiter == QChar{','});
    QCOMPARE(hyg->requiredColumns.size(), 3U);
    QCOMPARE(hyg->requiredColumns.at(0), QString("ra"));

    const CatalogSchemaDescriptor* openNgc = CatalogSchemaRegistry::find(CatalogSourceType::OpenNgcCsv);
    QVERIFY(openNgc != nullptr);
    QCOMPARE(openNgc->diagnosticName, std::string("OpenNGC CSV"));
    QVERIFY(openNgc->delimiter == QChar{';'});
    QCOMPARE(openNgc->requiredColumns.size(), 4U);

    const CatalogSchemaDescriptor* bundled = CatalogSchemaRegistry::find(CatalogSourceType::Bundled);
    QVERIFY(bundled != nullptr);
    QVERIFY(bundled->requiredColumns.empty());

    QVERIFY(CatalogSchemaRegistry::find(CatalogSourceType::Unknown) == nullptr);

    QVERIFY(CatalogSchemaRegistry::createParser(CatalogSourceType::HygCsv) != nullptr);
    QVERIFY(CatalogSchemaRegistry::createParser(CatalogSourceType::OpenNgcCsv) != nullptr);
    QVERIFY(CatalogSchemaRegistry::createParser(CatalogSourceType::Bundled) != nullptr);
}

void CatalogSchemaRegistryTests::registeredSchemaDrivesDetectionAndSelection()
{
    using namespace skygate::ephemeris;

    CatalogSchemaRegistry::registerSchema(
        CatalogSchemaDescriptor{
            .type = kTestSchemaType,
            .diagnosticName = "Test CSV",
            .delimiter = '|',
            .requiredColumns = {QStringLiteral("foo"), QStringLiteral("bar")},
            .createParser = []() { return std::make_unique<TestCatalogParser>(); },
        }
    );

    const CatalogSchemaDescriptor* descriptor = CatalogSchemaRegistry::find(kTestSchemaType);
    QVERIFY(descriptor != nullptr);
    QCOMPARE(descriptor->diagnosticName, std::string("Test CSV"));
    QVERIFY(descriptor->delimiter == QChar{'|'});
    QCOMPARE(descriptor->requiredColumns.size(), 2U);
    QCOMPARE(descriptor->requiredColumns.at(0), QString("foo"));
    QCOMPARE(CatalogSchemaRegistry::diagnosticName(kTestSchemaType), std::string("Test CSV"));

    QCOMPARE(CatalogPayloadFormatDetector::detect("foo|bar\n1|2\n"), kTestSchemaType);

    std::unique_ptr<ICatalogParser> parser = CatalogSchemaRegistry::createParser(kTestSchemaType);
    QVERIFY(parser != nullptr);
    const CatalogBodyParseResult result = parser->parse("foo|bar\n1|2\n", {});
    QVERIFY(result.isSuccess());
    QCOMPARE(result.bodies.size(), 1U);
    QCOMPARE(result.bodies.front().id, std::string("test_1"));
}

void CatalogSchemaRegistryTests::reportsUnknownSchemas()
{
    using namespace skygate::ephemeris;

    QVERIFY(CatalogSchemaRegistry::createParser(CatalogSourceType::Unknown) == nullptr);
    QCOMPARE(CatalogSchemaRegistry::diagnosticName(CatalogSourceType::Unknown), std::string("unknown"));
}

QTEST_APPLESS_MAIN(CatalogSchemaRegistryTests)

#include "CatalogSchemaRegistryTests.moc"
