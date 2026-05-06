#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QtTest/QtTest>

#include <string>

class StellariumConstellationParserTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsLegacyRowsPayload();
    void rejectsEmptyWhitespaceAndMalformedJsonPayloads();
    void parsesIndexJsonPayload();
    void parsesObjectRootWithoutConstellationArray();
    void providesBundledFallbackData();
};

void StellariumConstellationParserTests::rejectsLegacyRowsPayload()
{
    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(
        "ori 2 24436 25930 25930 26727\n"
        "uma 1 67301 65378\n"
    );

    QVERIFY(result.constellationCount == 0U);
    QVERIFY(result.lineRefs.empty());
    QVERIFY(result.labelRefs.empty());
}

void StellariumConstellationParserTests::rejectsEmptyWhitespaceAndMalformedJsonPayloads()
{
    const skygate::ephemeris::StellariumConstellationParser parser;

    for (const std::string_view payload : {"", "   \n\t  ", "[1, 2, 3]", "{ not json" }) {
        const auto result = parser.parse(payload);

        QVERIFY(result.constellationCount == 0U);
        QVERIFY(result.lineRefs.empty());
        QVERIFY(result.labelRefs.empty());
    }
}

void StellariumConstellationParserTests::parsesIndexJsonPayload()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": [
            {
                "id": "orion",
                "common_name": { "english": "Orion" },
                "lines": [[24436, 25930, 26727]]
            }
        ]
    })";

    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(kJsonPayload);

    QVERIFY(result.constellationCount == 1U);
    QVERIFY(result.lineRefs.size() == 2U);
    QVERIFY(result.labelRefs.size() == 1U);
    QVERIFY(result.labelRefs[0].first == "Orion");
    QVERIFY(result.labelRefs[0].second.size() == 3U);
}

void StellariumConstellationParserTests::parsesObjectRootWithoutConstellationArray()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "lines": [[1, 2, 3]],
            "line": [[4, 5]]
        }
    })";

    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(kJsonPayload);

    QVERIFY(result.constellationCount == 0U);
    QVERIFY(result.lineRefs.size() == 3U);
    QVERIFY(result.labelRefs.empty());
}

void StellariumConstellationParserTests::providesBundledFallbackData()
{
    const skygate::ephemeris::BundledConstellationData bundledData;
    const auto lineRefs = bundledData.lineRefs();
    const auto labelRefs = bundledData.labelRefs();

    QVERIFY(!lineRefs.empty());
    QVERIFY(!labelRefs.empty());
}

QTEST_APPLESS_MAIN(StellariumConstellationParserTests)

#include "StellariumConstellationParserTests.moc"
