#pragma once

#include "catalog/composition/DeepSkyCatalogMergeResult.hpp"
#include "Types.hpp"

#include <span>

namespace skygate::ephemeris {

class DeepSkyCatalogMerger final {
public:
    [[nodiscard]] static DeepSkyCatalogMergeResult merge(
        std::span<const CelestialBody> activeBodies,
        std::span<const CatalogCompositionSource> activeSourceKinds,
        std::span<const CelestialBody> deepSkyBodies
    );
};

}  // namespace skygate::ephemeris
