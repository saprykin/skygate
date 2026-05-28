#pragma once

#include "ActiveCatalogCompositionRequest.hpp"
#include "ActiveCatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    [[nodiscard]] static ActiveCatalogCompositionResult compose(const ActiveCatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
