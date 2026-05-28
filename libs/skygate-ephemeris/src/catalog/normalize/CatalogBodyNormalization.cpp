#include "CatalogBodyNormalization.hpp"
#include "StringUtilities.hpp"

namespace skygate::ephemeris {

void CatalogBodyNormalization::apply(OwnGalaxyCelestialBody& body)
{
    const std::string normalizedId = StringUtilities::toLowerAscii(body.id);
    if (normalizedId == "sun") {
        body.kind = BaseCelestialBody::Kind::Sun;
        return;
    }
    if (normalizedId == "moon") {
        body.kind = BaseCelestialBody::Kind::Moon;
        return;
    }

    (void)body;
}

void CatalogBodyNormalization::apply(std::vector<OwnGalaxyCelestialBody>& bodies)
{
    for (OwnGalaxyCelestialBody& body : bodies) {
        apply(body);
    }
}

}  // namespace skygate::ephemeris
