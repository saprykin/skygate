#pragma once

#include "IStarCatalog.hpp"

#include <span>

namespace skygate::ephemeris {

// Eager in-memory implementation of IStarCatalog.
//
// Owns a CelestialBodyCatalog snapshot by value and serves the common body
// view directly from it. This is the production provider for parsed, composed,
// augmented, and restored catalogs.
class InMemoryStarCatalog final : public IStarCatalog {
public:
    explicit InMemoryStarCatalog(CelestialBodyCatalog catalog);

    [[nodiscard]] const CelestialBodyCatalog& catalog() const noexcept override;
    [[nodiscard]] std::span<const BaseCelestialBody* const> bodies() const override;

private:
    CelestialBodyCatalog m_catalog;
};

}  // namespace skygate::ephemeris
