#include "OwnGalaxyCelestialBody.hpp"

namespace skygate::ephemeris {

const std::optional<CatalogStarAstrometry>& OwnGalaxyCelestialBody::starAstrometryValue() const noexcept
{
    return starAstrometry;
}

}  // namespace skygate::ephemeris
