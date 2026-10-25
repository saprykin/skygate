#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QString>

#include <span>

namespace skygate::ui::tests {

// Matches a presented display name against a catalog body list.
[[nodiscard]] bool catalogContainsDisplayName(
    std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const QString& displayName
);

// Read-only lookups over a composed star catalog by display name or object id.
class CatalogBodyQuery final {
public:
    [[nodiscard]] static bool
    containsDisplayName(const skygate::ephemeris::IStarCatalog* catalog, const QString& displayName);
    [[nodiscard]] static bool containsId(const skygate::ephemeris::IStarCatalog* catalog, const QString& objectId);
};

}  // namespace skygate::ui::tests
