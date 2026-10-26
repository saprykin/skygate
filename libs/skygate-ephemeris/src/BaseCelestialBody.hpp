#pragma once

#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogObjectIdentity.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

    [[nodiscard]] static constexpr std::string_view kindLabel(const Kind kind) noexcept
    {
        switch (kind) {
        case Kind::Star:
            return "Star";
        case Kind::Planet:
            return "Planet";
        case Kind::Moon:
            return "Moon";
        case Kind::Sun:
            return "Sun";
        case Kind::Constellation:
            return "Constellation";
        case Kind::DeepSkyObject:
            return "Deep sky object";
        }

        return "Object";
    }

    virtual ~BaseCelestialBody();

    [[nodiscard]] virtual const std::optional<skygate::core::EquatorialCoordinate>&
    fixedEquatorialValue() const noexcept;
    [[nodiscard]] virtual const std::optional<CatalogStarAstrometry>& starAstrometryValue() const noexcept;
    [[nodiscard]] virtual const std::optional<DeepSkyObjectInfo>& deepSkyObjectValue() const noexcept;
    [[nodiscard]] virtual const skygate::core::EquatorialCoordinate* fixedEquatorialCoordinate() const noexcept;
    [[nodiscard]] virtual const CatalogStarAstrometry* catalogStarAstrometry() const noexcept;
    [[nodiscard]] virtual const DeepSkyObjectInfo* deepSkyObjectInfo() const noexcept;

    // True when the body carries a catalog fixed equatorial coordinate.
    [[nodiscard]] bool hasFixedEquatorial() const noexcept
    {
        return fixedEquatorialValue().has_value();
    }

    [[nodiscard]] bool isCatalogStar() const noexcept;

    // Copies the fields every catalog domain object shares. Optional metadata
    // specific to a storage domain stays with the owning body type.
    void copySharedFieldsTo(BaseCelestialBody& target) const;

    // Canonical domain object identity. Never derived from displayName or sky
    // position; planet IDs such as "mercury" remain their domain identity.
    std::string id;
    std::string displayName;
    Kind kind = Kind::Star;
    double visualMagnitude = 0.0;
    CatalogObjectIdentity identity;
    // Catalog coordinate, present when the catalog carries a fixed position for
    // the body. Both storage domains keep it here.
    std::optional<skygate::core::EquatorialCoordinate> fixedEquatorial;
};

}  // namespace skygate::ephemeris
