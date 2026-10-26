#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <optional>

namespace skygate::ephemeris {

class OwnGalaxyCelestialBody : public BaseCelestialBody {
public:
    [[nodiscard]] const std::optional<CatalogStarAstrometry>& starAstrometryValue() const noexcept override;

    std::optional<CatalogStarAstrometry> starAstrometry;
};

}  // namespace skygate::ephemeris
