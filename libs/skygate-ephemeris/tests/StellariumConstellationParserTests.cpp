#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/CatalogComposer.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

bool isHipRef(const std::string_view id)
{
    return id.starts_with("hip_");
}

std::unordered_set<std::string> catalogBodyIds(
    const std::span<const skygate::ephemeris::CelestialBody> bodies
)
{
    std::unordered_set<std::string> ids;
    ids.reserve(bodies.size());
    for (const skygate::ephemeris::CelestialBody& body : bodies) {
        ids.insert(body.id);
    }
    return ids;
}

}  // namespace

class StellariumConstellationParserTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsLegacyRowsPayload();
    void rejectsEmptyWhitespaceAndMalformedJsonPayloads();
    void parsesIndexJsonPayload();
    void parsesObjectRootWithoutConstellationArray();
    void providesFallbackData();
    void fallbackDataResolvesAgainstBundledReferenceStars();
    void fallbackDataSeparatesBundledRefsFromHygHipRefs();
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

void StellariumConstellationParserTests::providesFallbackData()
{
    const skygate::ephemeris::FallbackConstellationData fallbackData;
    const auto lineRefs = fallbackData.lineRefs();
    const auto labelRefs = fallbackData.labelRefs();

    QVERIFY(!lineRefs.empty());
    QVERIFY(!labelRefs.empty());
}

void StellariumConstellationParserTests::fallbackDataResolvesAgainstBundledReferenceStars()
{
    auto bundledCatalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(bundledCatalog != nullptr);

    auto result = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *bundledCatalog
    });
    QVERIFY(result.isSuccess());

    const std::unordered_set<std::string> activeIds = catalogBodyIds(result.catalog->bodies());
    const skygate::ephemeris::FallbackConstellationData fallbackData;
    const auto lineRefs = fallbackData.lineRefs();
    const auto labelRefs = fallbackData.labelRefs();

    QVERIFY(std::any_of(lineRefs.begin(), lineRefs.end(), [&activeIds](const auto& lineRef) {
        return activeIds.contains(lineRef.first) && activeIds.contains(lineRef.second);
    }));

    bool foundNonUrsaBundledLabel = false;
    for (const auto& labelRef : labelRefs) {
        const bool hasBundledAnchor = std::any_of(
            labelRef.second.begin(),
            labelRef.second.end(),
            [&activeIds](const std::string& anchorId) {
                return activeIds.contains(anchorId);
            }
        );
        if (!hasBundledAnchor) {
            continue;
        }

        foundNonUrsaBundledLabel = foundNonUrsaBundledLabel
            || (labelRef.first != "Ursa Major" && labelRef.first != "Ursa Minor");
    }
    QVERIFY2(foundNonUrsaBundledLabel, "Fallback labels should not resolve only Ursa entries");
}

void StellariumConstellationParserTests::fallbackDataSeparatesBundledRefsFromHygHipRefs()
{
    const skygate::ephemeris::FallbackConstellationData fallbackData;
    const auto lineRefs = fallbackData.lineRefs();
    const auto labelRefs = fallbackData.labelRefs();

    std::size_t bundledLineRefCount = 0;
    for (const auto& lineRef : lineRefs) {
        const bool firstIsHip = isHipRef(lineRef.first);
        const bool secondIsHip = isHipRef(lineRef.second);
        if (!firstIsHip || !secondIsHip) {
            ++bundledLineRefCount;
            QCOMPARE(lineRef.first, std::string("betelgeuse"));
            QCOMPARE(lineRef.second, std::string("rigel"));
        }
    }
    QCOMPARE(bundledLineRefCount, std::size_t {1});

    const auto orionLabel = std::find_if(labelRefs.begin(), labelRefs.end(), [](const auto& labelRef) {
        return labelRef.first == "Orion";
    });
    QVERIFY(orionLabel != labelRefs.end());
    QVERIFY(
        std::find(orionLabel->second.begin(), orionLabel->second.end(), "betelgeuse")
        != orionLabel->second.end()
    );
    QVERIFY(
        std::find(orionLabel->second.begin(), orionLabel->second.end(), "rigel")
        != orionLabel->second.end()
    );
    QVERIFY(std::any_of(orionLabel->second.begin(), orionLabel->second.end(), [](const std::string& id) {
        return isHipRef(id);
    }));
}

QTEST_APPLESS_MAIN(StellariumConstellationParserTests)

#include "StellariumConstellationParserTests.moc"
