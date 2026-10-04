#pragma once

#include "CatalogCompositionPolicy.hpp"
#include "IStarCatalog.hpp"

#include <string>

namespace skygate::ephemeris {

// One configured source in an ordered catalog composition collection.
//
// sourceId is the stable source instance identity used for per-body
// provenance. It is independent of display labels and must be non-empty and
// unique within a collection, compared through the normalization used for
// composed identity keys (trimmed, ASCII case folded).
// CatalogComposer::composeCollection rejects an invalid collection with an
// explicit diagnostic rather than conflating provenance. enabled disables a
// source without removing it. catalog supplies the bodies; policy selects how
// those bodies participate in the merge.
struct CatalogCompositionSourceEntry final {
    std::string sourceId;
    bool enabled = true;
    const IStarCatalog* catalog = nullptr;
    CatalogCompositionPolicy policy = CatalogCompositionPolicy::Merge;
};

}  // namespace skygate::ephemeris
