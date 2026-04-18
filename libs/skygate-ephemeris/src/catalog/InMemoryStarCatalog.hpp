#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <span>
#include <vector>

namespace skygate::ephemeris {

class InMemoryStarCatalog final : public IStarCatalog {
public:
    explicit InMemoryStarCatalog(std::vector<CelestialBody> bodies);

    [[nodiscard]] std::span<const CelestialBody> bodies() const override;

private:
    std::vector<CelestialBody> m_bodies;
};

}  // namespace skygate::ephemeris
