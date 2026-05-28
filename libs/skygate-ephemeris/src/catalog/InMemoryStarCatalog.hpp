#pragma once

#include "IStarCatalog.hpp"

#include <span>

namespace skygate::ephemeris {

class InMemoryStarCatalog final : public IStarCatalog {
public:
    explicit InMemoryStarCatalog(CelestialBodyCatalog catalog);

    [[nodiscard]] const CelestialBodyCatalog& catalog() const noexcept override;
    [[nodiscard]] std::span<const BaseCelestialBody* const> bodies() const override;

private:
    CelestialBodyCatalog m_catalog;
};

}  // namespace skygate::ephemeris
