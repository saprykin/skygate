#include "catalog/model/InMemoryStarCatalog.hpp"

#include <utility>

namespace skygate::ephemeris {

InMemoryStarCatalog::InMemoryStarCatalog(std::vector<CelestialBody> bodies)
    : m_bodies(std::move(bodies))
{
}

std::span<const CelestialBody> InMemoryStarCatalog::bodies() const
{
    return m_bodies;
}

}  // namespace skygate::ephemeris
