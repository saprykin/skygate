#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <vector>

namespace skygate::ephemeris {

class InMemoryStarCatalog final : public IStarCatalog {
public:
    explicit InMemoryStarCatalog(std::vector<CelestialBody> bodies);

    [[nodiscard]] std::vector<CelestialBody> bodies() const override;

private:
    std::vector<CelestialBody> m_bodies;
};

}  // namespace skygate::ephemeris
