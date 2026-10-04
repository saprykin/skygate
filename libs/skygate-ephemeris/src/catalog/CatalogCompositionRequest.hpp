#pragma once

#include "CatalogCompositionSourceEntry.hpp"

#include <cstddef>
#include <vector>

namespace skygate::ephemeris {

// Ordered source collection for active catalog composition.
//
// Sources compose in collection order. currentConstellationCount preserves an
// already-known constellation count when the composed catalog reports fewer;
// knownDeepSkyObjectCount preserves a source-reported deep-sky object count
// for consumers that need the pre-merge figure.
struct CatalogCompositionRequest final {
    std::vector<CatalogCompositionSourceEntry> sources;
    std::size_t currentConstellationCount = 0;
    std::size_t knownDeepSkyObjectCount = 0;
};

}  // namespace skygate::ephemeris
