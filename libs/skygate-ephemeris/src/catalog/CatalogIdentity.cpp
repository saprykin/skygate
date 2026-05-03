#include "catalog/CatalogIdentity.hpp"

#include "common/StringUtilities.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace skygate::ephemeris::catalog_identity {
namespace {

void appendDeepSkyAliasKeys(std::vector<std::string>& keys, const CelestialBody& body)
{
    const auto appendKey = [&keys](const std::string_view value) {
        strings::appendUnique(keys, strings::normalizedAlnumKey(value));
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

}  // namespace

bool isReferenceLineStarId(const std::string_view id)
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

    const std::string loweredId = strings::toLowerAscii(id);
    return std::any_of(
        kReferenceLineStarIds.begin(),
        kReferenceLineStarIds.end(),
        [&loweredId](const std::string_view referenceId) {
            return loweredId == referenceId;
        }
    );
}

bool containsBodyId(
    const std::span<const CelestialBody> bodies,
    const std::string_view id
)
{
    return std::any_of(bodies.begin(), bodies.end(), [id](const CelestialBody& body) {
        return strings::equalsIgnoreAsciiCase(body.id, id);
    });
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

bool isAnalyticSolarSystemBody(const CelestialBody& body) noexcept
{
    return body.ephemerisSource == CelestialBodyEphemerisSource::Sun
        || body.ephemerisSource == CelestialBodyEphemerisSource::Moon
        || body.ephemerisSource == CelestialBodyEphemerisSource::Planet;
}

std::size_t countDeepSkyObjects(const std::span<const CelestialBody> bodies)
{
    return static_cast<std::size_t>(std::count_if(
        bodies.begin(),
        bodies.end(),
        [](const CelestialBody& body) {
            return body.type == CelestialBodyType::DeepSkyObject;
        }
    ));
}

}  // namespace skygate::ephemeris::catalog_identity
