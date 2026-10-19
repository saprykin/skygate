#pragma once

#include "catalog/CatalogCompositionMergeResult.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogMergeAccumulator.hpp"
#include "catalog/CatalogSourcePrecedence.hpp"

namespace skygate::ephemeris {

// Composition merge driver for catalog source collections.
//
// The merge contract is documented on CatalogIdentityAbsorption. This class owns
// one merge run: the configured source precedence, the survivor accumulator the
// merge stages fill, the identity decision, and the assembly of the result.
class CatalogCompositionMerger final {
public:
    [[nodiscard]] static CatalogCompositionMergeResult mergeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
