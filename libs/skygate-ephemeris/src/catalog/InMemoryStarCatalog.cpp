#include "InMemoryStarCatalog.hpp"

#include <utility>

namespace skygate::ephemeris {

InMemoryStarCatalog::InMemoryStarCatalog(CelestialBodyCatalog catalog) : m_catalog(std::move(catalog)) {}

const CelestialBodyCatalog& InMemoryStarCatalog::catalog() const noexcept
{
    return m_catalog;
}

std::span<const BaseCelestialBody* const> InMemoryStarCatalog::bodies() const
{
    return m_catalog.bodies();
}

}  // namespace skygate::ephemeris
