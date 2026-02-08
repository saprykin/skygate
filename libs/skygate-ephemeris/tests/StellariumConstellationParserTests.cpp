#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>

class StellariumConstellationParserTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesLegacyRowsPayload();
    void parsesIndexJsonPayload();
    void providesBundledFallbackData();
};

void StellariumConstellationParserTests::parsesLegacyRowsPayload()
{
    const skygate::ephemeris::StellariumConstellationParser parser;
    const auto result = parser.parse(
        "ori 2 24436 25930 25930 26727\n"
        "uma 1 67301 65378\n"
    );

    QVERIFY(!result.isJsonPayload);
    QVERIFY(result.constellationCount == 2U);
    QVERIFY(result.lineRefs.size() == 3U);

    const auto hasLine = [&result](const std::string& startId, const std::string& endId) {
        return std::any_of(result.lineRefs.begin(), result.lineRefs.end(), [&](const auto& lineRef) {
            return lineRef.first == startId && lineRef.second == endId;
        });
    };

    QVERIFY(hasLine("hip_24436", "hip_25930"));
    QVERIFY(hasLine("hip_25930", "hip_26727"));
    QVERIFY(hasLine("hip_67301", "hip_65378"));
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

    QVERIFY(result.isJsonPayload);
    QVERIFY(result.constellationCount == 1U);
    QVERIFY(result.lineRefs.size() == 2U);
    QVERIFY(result.labelRefs.size() == 1U);
    QVERIFY(result.labelRefs[0].first == "Orion");
    QVERIFY(result.labelRefs[0].second.size() == 3U);
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
