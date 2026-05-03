#include "TestHelpers.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <QtTest/QtTest>

#include <string>

class HygCatalogParserTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesBasicRows();
    void supportsFallbackIdsAndQuotedFields();
    void keepsWholeCatalogByDefault();
    void rejectsMalformedInput();
};

void HygCatalogParserTests::parsesBasicRows()
{
    auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,hip,proper,ra,dec,mag\n"
        "1,32349,Sirius,6.7525,-16.7161,-1.46\n"
        "2,24608,Capella,5.2782,45.9979,0.08\n"
    );
    QVERIFY(result.isSuccess());
    const auto& catalog = result.catalog;
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 2U);
    QVERIFY(bodies[0].id == "hip_32349");
    QVERIFY(bodies[0].fixedEquatorial.has_value());
}

void HygCatalogParserTests::supportsFallbackIdsAndQuotedFields()
{
    auto result = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "hip,id,proper,bf,ra,dec,mag\n"
        ",,Unnamed,,1.0,2.0,3.0\n"
        ",99,,Beta,4.0,5.0,6.0\n"
        "77,,,,7.0,8.0,9.0\n"
        "2,101,\"Alpha, Beta\",,10.0,11.0,1.5\n"
        "3,102,\"Quote \"\"Star\"\"\",,12.0,13.0,2.5\n"
        "1,100,Skipped,,1.0,2.0,\n"
    );
    QVERIFY(result.isSuccess());
    const auto& catalog = result.catalog;
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 5U);

    using namespace skygate::ephemeris::tests;
    const auto* autoBody = findBodyById(bodies, "hyg_auto_1");
    QVERIFY(autoBody != nullptr);
    QVERIFY(autoBody->displayName == "Unnamed");

    const auto* hygIdBody = findBodyById(bodies, "hyg_99");
    QVERIFY(hygIdBody != nullptr);
    QVERIFY(hygIdBody->displayName == "Beta");

    const auto* hipBody = findBodyById(bodies, "hip_77");
    QVERIFY(hipBody != nullptr);
    QVERIFY(hipBody->displayName == "HIP 77");

    const auto* quotedNameBody = findBodyById(bodies, "hip_2");
    QVERIFY(quotedNameBody != nullptr);
    QVERIFY(quotedNameBody->displayName == "Alpha, Beta");

    const auto* escapedQuoteBody = findBodyById(bodies, "hip_3");
    QVERIFY(escapedQuoteBody != nullptr);
    QVERIFY(escapedQuoteBody->displayName == "Quote \"Star\"");
    QVERIFY(escapedQuoteBody->fixedEquatorial.has_value());
}

void HygCatalogParserTests::keepsWholeCatalogByDefault()
{
    std::string csv = "hip,id,proper,bf,ra,dec,mag\n";
    csv.reserve(1024U * 1024U);

    for (int index = 1; index <= 30000; ++index) {
        csv += std::to_string(index);
        csv += ",";
        csv += std::to_string(index);
        csv += ",Star ";
        csv += std::to_string(index);
        csv += ",,";
        csv += std::to_string(index % 24);
        csv += ",";
        csv += std::to_string((index % 180) - 90);
        csv += ",";
        csv += std::to_string(index);
        csv += "\n";
    }

    std::size_t lastProgress = 0;
    auto loadResult = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        csv,
        [&lastProgress](const std::size_t parsedObjectCount) {
            lastProgress = parsedObjectCount;
        }
    );
    QVERIFY(loadResult.isSuccess());
    QVERIFY(lastProgress == 30000U);
    QVERIFY(loadResult.diagnostics.parsedBodyCount == 30000U);
    QVERIFY(loadResult.diagnostics.selectedBodyCount == 30000U);
    QVERIFY(loadResult.diagnostics.truncatedBodyCount == 0U);

    const auto bodies = loadResult.catalog->bodies();
    QVERIFY(bodies.size() == 30000U);

    using namespace skygate::ephemeris::tests;
    QVERIFY(findBodyById(bodies, "hip_1") != nullptr);
    QVERIFY(findBodyById(bodies, "hip_25000") != nullptr);
    QVERIFY(findBodyById(bodies, "hip_25001") != nullptr);
    QVERIFY(findBodyById(bodies, "hip_30000") != nullptr);
}

void HygCatalogParserTests::rejectsMalformedInput()
{
    const auto headerOnlyResult = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "hip,id,proper,ra,dec,mag\n"
    );
    QVERIFY(!headerOnlyResult.isSuccess());

    const auto malformedResult = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        "id,name\n"
        "1,NoCoordinates\n"
    );
    QVERIFY(!malformedResult.isSuccess());
}

QTEST_APPLESS_MAIN(HygCatalogParserTests)

#include "HygCatalogParserTests.moc"
