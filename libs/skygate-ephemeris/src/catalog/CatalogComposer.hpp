#pragma once

#include "ActiveCatalogCompositionRequest.hpp"
#include "ActiveCatalogCompositionResult.hpp"
#include "CatalogCompositionRequest.hpp"
#include "CatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    // Legacy two-slot compatibility adapter. It expresses the old primary plus
    // deep-sky pair as an ordered source collection and delegates to
    // composeCollection, then maps the per-source provenance back to the
    // legacy source categories.
    [[nodiscard]] static ActiveCatalogCompositionResult compose(const ActiveCatalogCompositionRequest& request);

    // Ordered source collection composition. Sources compose in collection
    // order using the shared identity-resolution and merge policy.
    [[nodiscard]] static CatalogCompositionResult composeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
