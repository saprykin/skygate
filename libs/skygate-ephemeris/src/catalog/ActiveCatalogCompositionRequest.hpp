#pragma once

#include "catalog/IStarCatalog.hpp"

#include <cstddef>

namespace skygate::ephemeris {

struct ActiveCatalogCompositionRequest final {
    const IStarCatalog& sourceCatalog;
    const IStarCatalog* deepSkyCatalog = nullptr;
    bool useBundledDeepSkyCatalog = false;
    std::size_t currentConstellationCount = 0;
    std::size_t knownDeepSkyObjectCount = 0;
};

}  // namespace skygate::ephemeris
