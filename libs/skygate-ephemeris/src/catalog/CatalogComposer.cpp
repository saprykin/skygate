#include "skygate/ephemeris/CatalogComposer.hpp"

#include "skygate/ephemeris/CatalogFactory.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

std::string toLowerId(const std::string& value)
{
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lower;
}

bool isSunOrMoonType(const CelestialBodyType type)
{
    return type == CelestialBodyType::Sun || type == CelestialBodyType::Moon;
}

bool isReferenceLineStarId(const std::string& id)
{
    constexpr std::array<std::string_view, 8> kReferenceLineStarIds = {{
        "sirius",
        "canopus",
        "arcturus",
        "vega",
        "capella",
        "rigel",
        "procyon",
        "betelgeuse",
    }};

    const std::string loweredId = toLowerId(id);
    return std::any_of(kReferenceLineStarIds.begin(), kReferenceLineStarIds.end(), [&loweredId](const auto value) {
        return loweredId == value;
    });
}

bool containsBodyIdCaseInsensitive(
    const std::vector<CelestialBody>& bodies,
    const std::string& id
)
{
    const std::string loweredId = toLowerId(id);
    return std::any_of(bodies.begin(), bodies.end(), [&loweredId](const auto& body) {
        return toLowerId(body.id) == loweredId;
    });
}

void appendDeepSkyAliasKeys(std::vector<std::string>& keys, const CelestialBody& body)
{
    const auto appendKey = [&keys](std::string value) {
        value = toLowerId(value);
        value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char c) {
            return !std::isalnum(c);
        }), value.end());
        if (!value.empty() && std::find(keys.begin(), keys.end(), value) == keys.end()) {
            keys.push_back(std::move(value));
        }
    };

    appendKey(body.id);
    appendKey(body.displayName);
    if (!body.deepSkyObject.has_value()) {
        return;
    }

    for (const auto& alias : body.deepSkyObject->aliases) {
        appendKey(alias);
    }
}

bool sharesDeepSkyAlias(const CelestialBody& lhs, const CelestialBody& rhs)
{
    std::vector<std::string> lhsKeys;
    std::vector<std::string> rhsKeys;
    appendDeepSkyAliasKeys(lhsKeys, lhs);
    appendDeepSkyAliasKeys(rhsKeys, rhs);
    return std::any_of(lhsKeys.begin(), lhsKeys.end(), [&rhsKeys](const std::string& key) {
        return std::find(rhsKeys.begin(), rhsKeys.end(), key) != rhsKeys.end();
    });
}

}  // namespace

std::unique_ptr<IStarCatalog> createCatalogByMergingDeepSkyObjects(
    const IStarCatalog& baseCatalog,
    const IStarCatalog& deepSkyCatalog
)
{
    return createCatalogByMergingDeepSkyObjects(baseCatalog.bodies(), deepSkyCatalog.bodies());
}

std::unique_ptr<IStarCatalog> createCatalogByMergingDeepSkyObjects(
    const std::span<const CelestialBody> baseBodies,
    const std::span<const CelestialBody> deepSkyBodies
)
{
    std::vector<CelestialBody> mergedBodies;
    mergedBodies.reserve(baseBodies.size() + deepSkyBodies.size());
    for (const auto& body : baseBodies) {
        if (body.type != CelestialBodyType::DeepSkyObject) {
            mergedBodies.push_back(body);
        }
    }

    for (const auto& body : baseBodies) {
        if (body.type != CelestialBodyType::DeepSkyObject) {
            continue;
        }

        const bool replacedByDownloadedDeepSkyObject = std::any_of(
            deepSkyBodies.begin(),
            deepSkyBodies.end(),
            [&body](const CelestialBody& deepSkyBody) {
                return deepSkyBody.type == CelestialBodyType::DeepSkyObject
                    && sharesDeepSkyAlias(body, deepSkyBody);
            }
        );
        if (!replacedByDownloadedDeepSkyObject) {
            mergedBodies.push_back(body);
        }
    }

    for (const auto& body : deepSkyBodies) {
        if (body.type == CelestialBodyType::DeepSkyObject) {
            mergedBodies.push_back(body);
        }
    }

    return createStarCatalogFromBodies(std::move(mergedBodies));
}

std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(const IStarCatalog& catalog)
{
    return createCatalogWithCoreSolarSystemBodies(catalog.bodies());
}

std::unique_ptr<IStarCatalog> createCatalogWithCoreSolarSystemBodies(
    const std::span<const CelestialBody> bodies
)
{
    std::vector<CelestialBody> mergedBodies(bodies.begin(), bodies.end());
    const std::size_t starCount = static_cast<std::size_t>(std::count_if(
        mergedBodies.begin(),
        mergedBodies.end(),
        [](const auto& body) {
            return body.type == CelestialBodyType::Star;
        }
    ));

    std::unique_ptr<IStarCatalog> bundledCatalog = createBundledStarCatalog();
    if (bundledCatalog == nullptr) {
        return createStarCatalogFromBodies(std::move(mergedBodies));
    }

    for (const auto& body : bundledCatalog->bodies()) {
        const bool isReferenceLineStar = body.type == CelestialBodyType::Star
            && isReferenceLineStarId(body.id)
            && starCount == 0U;
        if (
            !isSunOrMoonType(body.type)
            && body.type != CelestialBodyType::Planet
            && body.type != CelestialBodyType::DeepSkyObject
            && !isReferenceLineStar
        ) {
            continue;
        }

        if (containsBodyIdCaseInsensitive(mergedBodies, body.id)) {
            continue;
        }

        mergedBodies.push_back(body);
    }

    return createStarCatalogFromBodies(std::move(mergedBodies));
}

}  // namespace skygate::ephemeris
