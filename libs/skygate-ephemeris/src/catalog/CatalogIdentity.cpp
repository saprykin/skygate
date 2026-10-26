#include "CatalogIdentity.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace skygate::ephemeris {

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

std::span<const CatalogIdentifier> CatalogIdentity::externalIdentifiers(const BaseCelestialBody& body) noexcept
{
    return body.identity.externalIdentifiers;
}

std::vector<CatalogIdentifier> CatalogIdentity::authoritativeDesignations(const BaseCelestialBody& body)
{
    std::vector<CatalogIdentifier> designations;
    designations.reserve(body.identity.externalIdentifiers.size() + 1U);
    for (const CatalogIdentifier& identifier : externalIdentifiers(body)) {
        CatalogIdentifier::appendUnique(designations, identifier);
    }

    if (const std::optional<CatalogIdentifier> designation = CatalogIdentifier::designationFromCanonicalId(body.id);
        designation.has_value()) {
        CatalogIdentifier::appendUnique(designations, *designation);
    }
    return designations;
}

}  // namespace skygate::ephemeris
