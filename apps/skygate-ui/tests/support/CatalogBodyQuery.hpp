#pragma once

#include "catalog/IStarCatalog.hpp"

#include <QString>

namespace skygate::ui::tests {

// Read-only lookups over a composed star catalog by display name or object id.
class CatalogBodyQuery final {
public:
    [[nodiscard]] static bool
    containsDisplayName(const skygate::ephemeris::IStarCatalog* catalog, const QString& displayName);
    [[nodiscard]] static bool containsId(const skygate::ephemeris::IStarCatalog* catalog, const QString& objectId);
};

}  // namespace skygate::ui::tests
