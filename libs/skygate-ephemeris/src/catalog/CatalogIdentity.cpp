#include "CatalogIdentity.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace skygate::ephemeris {
namespace {

void appendDeepSkyAliasKeys(std::vector<std::string>& keys, const BaseCelestialBody& body)
{
    const auto appendKey = [&keys](const std::string_view value) {
        StringUtilities::appendUnique(keys, StringUtilities::normalizedAlnumKey(value));
    };

    appendKey(body.id);
    appendKey(body.displayName);
    const std::optional<DeepSkyObjectInfo>& deepSkyObject = body.deepSkyObjectValue();
    if (!deepSkyObject.has_value()) {
        return;
    }

    for (const auto& alias : deepSkyObject->aliases) {
        appendKey(alias);
    }
}

}  // namespace

bool CatalogIdentity::containsBodyId(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    return std::any_of(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && StringUtilities::equalsIgnoreAsciiCase(body->id, id);
    });
}

bool CatalogIdentity::sharesDeepSkyAlias(const BaseCelestialBody& lhs, const BaseCelestialBody& rhs)
{
    std::vector<std::string> lhsKeys;
    std::vector<std::string> rhsKeys;
    appendDeepSkyAliasKeys(lhsKeys, lhs);
    appendDeepSkyAliasKeys(rhsKeys, rhs);
    return std::any_of(lhsKeys.begin(), lhsKeys.end(), [&rhsKeys](const std::string& key) {
        return std::find(rhsKeys.begin(), rhsKeys.end(), key) != rhsKeys.end();
    });
}

bool CatalogIdentity::isAnalyticSolarSystemBody(const BaseCelestialBody& body) noexcept
{
    return body.kind == BaseCelestialBody::Kind::Sun || body.kind == BaseCelestialBody::Kind::Moon
           || body.kind == BaseCelestialBody::Kind::Planet;
}

std::size_t CatalogIdentity::countDeepSkyObjects(const std::span<const BaseCelestialBody* const> bodies)
{
    return static_cast<std::size_t>(std::count_if(bodies.begin(), bodies.end(), [](const BaseCelestialBody* body) {
        return body != nullptr && body->kind == BaseCelestialBody::Kind::DeepSkyObject;
    }));
}

}  // namespace skygate::ephemeris
