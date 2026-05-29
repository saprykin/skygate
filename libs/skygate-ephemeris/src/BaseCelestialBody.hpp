#pragma once

#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace skygate::ephemeris {

class BaseCelestialBody {
public:
    enum class Kind : std::uint8_t {
        Star,
        Planet,
        Moon,
        Sun,
        Constellation,
        DeepSkyObject
    };

    virtual ~BaseCelestialBody();

    [[nodiscard]] virtual const std::optional<skygate::core::EquatorialCoordinate>&
    fixedEquatorialValue() const noexcept;
    [[nodiscard]] virtual const std::optional<CatalogStarAstrometry>& starAstrometryValue() const noexcept;
    [[nodiscard]] virtual const std::optional<DeepSkyObjectInfo>& deepSkyObjectValue() const noexcept;
    [[nodiscard]] virtual const skygate::core::EquatorialCoordinate* fixedEquatorialCoordinate() const noexcept;
    [[nodiscard]] virtual const CatalogStarAstrometry* catalogStarAstrometry() const noexcept;
    [[nodiscard]] virtual const DeepSkyObjectInfo* deepSkyObjectInfo() const noexcept;

    std::string id;
    std::string displayName;
    Kind kind = Kind::Star;
    double visualMagnitude = 0.0;
};

}  // namespace skygate::ephemeris
