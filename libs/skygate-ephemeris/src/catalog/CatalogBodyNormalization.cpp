#include "catalog/CatalogBodyNormalization.hpp"

#include "common/StringUtilities.hpp"

namespace skygate::ephemeris {

void CatalogBodyNormalization::apply(CelestialBody& body)
{
    if (body.fixedEquatorial.has_value()) {
        body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
        return;
    }

    const std::string normalizedId = strings::toLowerAscii(body.id);
    if (normalizedId == "sun") {
        body.ephemerisSource = CelestialBodyEphemerisSource::Sun;
        return;
    }
    if (normalizedId == "moon") {
        body.ephemerisSource = CelestialBodyEphemerisSource::Moon;
        return;
    }

    switch (body.type) {
    case CelestialBodyType::Sun:
        body.ephemerisSource = CelestialBodyEphemerisSource::Sun;
        break;
    case CelestialBodyType::Moon:
        body.ephemerisSource = CelestialBodyEphemerisSource::Moon;
        break;
    case CelestialBodyType::Planet:
        body.ephemerisSource = CelestialBodyEphemerisSource::Planet;
        break;
    case CelestialBodyType::Star:
        body.ephemerisSource = CelestialBodyEphemerisSource::Star;
        break;
    case CelestialBodyType::Constellation:
        body.ephemerisSource = CelestialBodyEphemerisSource::Constellation;
        break;
    case CelestialBodyType::DeepSkyObject:
        body.ephemerisSource = body.fixedEquatorial.has_value()
            ? CelestialBodyEphemerisSource::FixedEquatorial
            : CelestialBodyEphemerisSource::Unresolved;
        break;
    }
}

void CatalogBodyNormalization::apply(std::vector<CelestialBody>& bodies)
{
    for (CelestialBody& body : bodies) {
        apply(body);
    }
}

}  // namespace skygate::ephemeris
