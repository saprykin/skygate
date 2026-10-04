#pragma once

#include "CatalogCompositionRequest.hpp"
#include "CatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    // Ordered source collection composition. Sources compose in collection
    // order using the shared identity-resolution and merge policy.
    [[nodiscard]] static CatalogCompositionResult composeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
