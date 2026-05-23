#include "catalog/CatalogIdentity.hpp"

#include "StringUtilities.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace skygate::ephemeris {
namespace {

void appendDeepSkyAliasKeys(std::vector<std::string>& keys, const CelestialBody& body)
{
    const auto appendKey = [&keys](const std::string_view value) {
        StringUtilities::appendUnique(keys, StringUtilities::normalizedAlnumKey(value));
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

bool CatalogIdentity::containsBodyId(const std::span<const CelestialBody> bodies, const std::string_view id)
{
    return std::any_of(bodies.begin(), bodies.end(), [id](const CelestialBody& body) {
        return StringUtilities::equalsIgnoreAsciiCase(body.id, id);
    });
}

bool CatalogIdentity::sharesDeepSkyAlias(const CelestialBody& lhs, const CelestialBody& rhs)
{
    std::vector<std::string> lhsKeys;
    std::vector<std::string> rhsKeys;
    appendDeepSkyAliasKeys(lhsKeys, lhs);
    appendDeepSkyAliasKeys(rhsKeys, rhs);
    return std::any_of(lhsKeys.begin(), lhsKeys.end(), [&rhsKeys](const std::string& key) {
        return std::find(rhsKeys.begin(), rhsKeys.end(), key) != rhsKeys.end();
    });
}

bool CatalogIdentity::isAnalyticSolarSystemBody(const CelestialBody& body) noexcept
{
    return body.ephemerisSource == CelestialBodyEphemerisSource::Sun
           || body.ephemerisSource == CelestialBodyEphemerisSource::Moon
           || body.ephemerisSource == CelestialBodyEphemerisSource::Planet;
}

std::size_t CatalogIdentity::countDeepSkyObjects(const std::span<const CelestialBody> bodies)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [](const CelestialBody& body) {
        return body.type == CelestialBodyType::DeepSkyObject;
    }));
}

}  // namespace skygate::ephemeris
