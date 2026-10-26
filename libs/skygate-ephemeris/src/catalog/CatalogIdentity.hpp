#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace skygate::ephemeris {

// Identity views shared by the catalog identity consumers.
//
// Two consumers decide which identifiers are authoritative. They agree on the
// starting point but not on the whole set:
//
//   - CatalogIdentityIndex registers a record's canonical id as its own key
//     family, so a canonical id matches only the same canonical id and a
//     retained canonical id keeps resolving after a replacement. Explicit
//     external identifiers form a second, equally authoritative family, and
//     that list is the one below.
//   - Composition's contradiction and ambiguity checks work per identifier
//     namespace, so they fold a recognized deep-sky canonical id into the
//     identifier set (authoritativeDesignations): two distinct NGC designations
//     that share a common name stay distinct even when one record carries only
//     the canonical id.
//
// Neither view demotes an authoritative identifier to a display alias.
class CatalogIdentity final {
public:
    [[nodiscard]] static bool isAnalyticSolarSystemBody(const BaseCelestialBody& body) noexcept;
    [[nodiscard]] static std::size_t countDeepSkyObjects(std::span<const BaseCelestialBody* const> bodies);

    // The explicit cross-identifiers a record carries, in declaration order.
    [[nodiscard]] static std::span<const CatalogIdentifier> externalIdentifiers(const BaseCelestialBody& body) noexcept;

    // Every designation a record authoritatively claims: its explicit external
    // identifiers plus the designation a recognized deep-sky canonical id was
    // derived from.
    [[nodiscard]] static std::vector<CatalogIdentifier> authoritativeDesignations(const BaseCelestialBody& body);
};

}  // namespace skygate::ephemeris
