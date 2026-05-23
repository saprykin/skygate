#pragma once

#include "skygate/ephemeris/ActiveCatalogCompositionRequest.hpp"
#include "skygate/ephemeris/ActiveCatalogCompositionResult.hpp"

namespace skygate::ephemeris {

class CatalogComposer final {
public:
    [[nodiscard]] static ActiveCatalogCompositionResult compose(const ActiveCatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
