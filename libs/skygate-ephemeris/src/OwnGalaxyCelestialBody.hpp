#pragma once

#include "BaseCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <optional>

namespace skygate::ephemeris {

class OwnGalaxyCelestialBody : public BaseCelestialBody {
public:
    [[nodiscard]] const std::optional<core::EquatorialCoordinate>& fixedEquatorialValue() const noexcept override;
    [[nodiscard]] const std::optional<CatalogStarAstrometry>& starAstrometryValue() const noexcept override;

    std::optional<core::EquatorialCoordinate> fixedEquatorial;
    std::optional<CatalogStarAstrometry> starAstrometry;
};

}  // namespace skygate::ephemeris
