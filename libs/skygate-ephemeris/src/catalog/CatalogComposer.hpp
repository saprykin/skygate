#pragma once

#include "catalog/ActiveCatalogCompositionRequest.hpp"
#include "catalog/ActiveCatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    [[nodiscard]] static ActiveCatalogCompositionResult compose(const ActiveCatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
