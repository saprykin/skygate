#include "EphemerisSnapshot.hpp"

namespace skygate::ephemeris {

std::span<const BaseCelestialBody* const> EphemerisSnapshot::bodies() const noexcept
{
    if (catalogBodies == nullptr) {
        return {};
    }

    return catalogBodies->bodies();
}

const BaseCelestialBody& EphemerisSnapshot::bodyAt(const std::size_t bodyIndex) const noexcept
{
    return catalogBodies->bodyAt(bodyIndex);
}

}  // namespace skygate::ephemeris
