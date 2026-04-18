#include "catalog/CatalogBodyNormalization.hpp"

#include <algorithm>
#include <cctype>

namespace skygate::ephemeris {
namespace {

std::string toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

void CatalogBodyNormalization::apply(CelestialBody& body)
{
    if (body.fixedEquatorial.has_value()) {
        body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
        return;
    }

    const std::string normalizedId = toLowerAscii(body.id);
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
    }
}

void CatalogBodyNormalization::apply(std::vector<CelestialBody>& bodies)
{
    for (CelestialBody& body : bodies) {
        apply(body);
    }
}

}  // namespace skygate::ephemeris
