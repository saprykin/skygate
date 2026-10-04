#pragma once

#include "CatalogCompositionRequest.hpp"
#include "CatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    // Ordered source collection composition. Sources compose in collection
    // order using the shared identity-resolution and merge policy.
    //
    // A collection whose source identities are empty or duplicated (after
    // trimming and ASCII case folding) is rejected: the returned result
    // carries ErrorCode::InvalidSourceIdentity and a diagnostic instead of
    // silently conflating provenance.
    [[nodiscard]] static CatalogCompositionResult composeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
