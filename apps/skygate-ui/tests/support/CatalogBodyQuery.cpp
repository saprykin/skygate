#include "CatalogBodyQuery.hpp"

#include <algorithm>

namespace skygate::ui::tests {

bool catalogContainsDisplayName(
    const std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies, const QString& displayName
)
{
    return std::any_of(bodies.begin(), bodies.end(), [&displayName](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->displayName) == displayName;
    });
}

bool CatalogBodyQuery::containsDisplayName(const skygate::ephemeris::IStarCatalog* catalog, const QString& displayName)
{
    if (catalog == nullptr) {
        return false;
    }

    return catalogContainsDisplayName(catalog->bodies(), displayName);
}

bool CatalogBodyQuery::containsId(const skygate::ephemeris::IStarCatalog* catalog, const QString& objectId)
{
    if (catalog == nullptr) {
        return false;
    }

    const auto bodies = catalog->bodies();
    return std::any_of(bodies.begin(), bodies.end(), [&objectId](const skygate::ephemeris::BaseCelestialBody* body) {
        return body != nullptr && QString::fromStdString(body->id) == objectId;
    });
}

}  // namespace skygate::ui::tests
