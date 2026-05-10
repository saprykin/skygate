#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/CatalogComposer.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"
#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <set>
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

std::unordered_set<std::string> nonHipFallbackIds(
    const skygate::ephemeris::FallbackConstellationData& fallbackData
)
{
    std::unordered_set<std::string> ids;
    for (const auto& lineRef : fallbackData.lineRefs()) {
        if (!isHipRef(lineRef.first)) {
            ids.insert(lineRef.first);
        }
        if (!isHipRef(lineRef.second)) {
            ids.insert(lineRef.second);
        }
    }
    for (const auto& labelRef : fallbackData.labelRefs()) {
        for (const std::string& anchorId : labelRef.second) {
            if (!isHipRef(anchorId)) {
                ids.insert(anchorId);
            }
        }
    }
    return ids;
}

std::set<std::string> hipFallbackIds(
    const skygate::ephemeris::FallbackConstellationData& fallbackData
)
{
    std::set<std::string> ids;
    for (const auto& lineRef : fallbackData.lineRefs()) {
        if (isHipRef(lineRef.first)) {
            ids.insert(lineRef.first);
        }
        if (isHipRef(lineRef.second)) {
            ids.insert(lineRef.second);
        }
    }
    for (const auto& labelRef : fallbackData.labelRefs()) {
        for (const std::string& anchorId : labelRef.second) {
            if (isHipRef(anchorId)) {
                ids.insert(anchorId);
            }
        }
    }
    return ids;
}

std::string makeHygCsvForHipIds(const std::set<std::string>& hipIds)
{
    std::string csv = "id,hip,proper,ra,dec,mag\n";
    std::size_t rowId = 1;
    for (const std::string& hipId : hipIds) {
        const std::string hipNumber = hipId.substr(std::string_view("hip_").size());
        csv += std::to_string(rowId++);
        csv += ",";
        csv += hipNumber;
        csv += ",HIP ";
        csv += hipNumber;
        csv += ",";
        csv += std::to_string(rowId % 24U);
        csv += ",";
        csv += std::to_string(static_cast<int>(rowId % 120U) - 60);
        csv += ",1.0\n";
    }
    return csv;
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
    void fallbackDataHipRefsResolveAgainstHygCatalogIds();
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

    const auto bundledFallbackIds = nonHipFallbackIds(fallbackData);
    QVERIFY(!bundledFallbackIds.empty());
    for (const std::string& id : bundledFallbackIds) {
        QVERIFY2(activeIds.contains(id), id.c_str());
    }

    std::size_t bundledLineRefCount = 0;
    for (const auto& lineRef : lineRefs) {
        if (isHipRef(lineRef.first) || isHipRef(lineRef.second)) {
            continue;
        }

        ++bundledLineRefCount;
        QVERIFY2(activeIds.contains(lineRef.first), lineRef.first.c_str());
        QVERIFY2(activeIds.contains(lineRef.second), lineRef.second.c_str());
    }
    QVERIFY(bundledLineRefCount > 0U);

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

void StellariumConstellationParserTests::fallbackDataHipRefsResolveAgainstHygCatalogIds()
{
    const skygate::ephemeris::FallbackConstellationData fallbackData;
    const std::set<std::string> hipIds = hipFallbackIds(fallbackData);
    QVERIFY(!hipIds.empty());

    const std::string csv = makeHygCsvForHipIds(hipIds);
    auto loadResult = skygate::ephemeris::loadStarCatalog(
        skygate::ephemeris::CatalogSourceType::HygCsv,
        csv
    );
    QVERIFY(loadResult.isSuccess());
    QVERIFY(loadResult.catalog != nullptr);

    auto activeCatalogResult = skygate::ephemeris::composeActiveCatalog({
        .sourceCatalog = *loadResult.catalog
    });
    QVERIFY(activeCatalogResult.isSuccess());

    const std::unordered_set<std::string> activeIds = catalogBodyIds(
        activeCatalogResult.catalog->bodies()
    );
    for (const std::string& id : hipIds) {
        QVERIFY2(activeIds.contains(id), id.c_str());
    }
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
