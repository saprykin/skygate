#include "CatalogIdentity.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <string_view>

namespace skygate::ephemeris {

bool CatalogIdentity::containsBodyId(const std::span<const BaseCelestialBody* const> bodies, const std::string_view id)
{
    return std::any_of(bodies.begin(), bodies.end(), [id](const BaseCelestialBody* body) {
        return body != nullptr && StringUtilities::equalsIgnoreAsciiCase(body->id, id);
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
