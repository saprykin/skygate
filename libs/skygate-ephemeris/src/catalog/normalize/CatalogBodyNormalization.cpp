#include "CatalogBodyNormalization.hpp"
#include "StringUtilities.hpp"
#include "catalog/CelestialBodyIds.hpp"

namespace skygate::ephemeris {

void CatalogBodyNormalization::apply(OwnGalaxyCelestialBody& body)
{
    const std::string normalizedId = StringUtilities::normalizedLookupKey(body.id);
    if (normalizedId == CelestialBodyIds::kSun) {
        body.kind = BaseCelestialBody::Kind::Sun;
        return;
    }
    if (normalizedId == CelestialBodyIds::kMoon) {
        body.kind = BaseCelestialBody::Kind::Moon;
        return;
    }
}

void CatalogBodyNormalization::apply(std::vector<OwnGalaxyCelestialBody>& bodies)
{
    for (OwnGalaxyCelestialBody& body : bodies) {
        apply(body);
    }
}

}  // namespace skygate::ephemeris
