#pragma once

#include "skygate/ephemeris/CatalogComposer.hpp"

#include <span>
#include <vector>

namespace skygate::ephemeris {

struct DeepSkyCatalogMergeResult final {
    std::vector<CelestialBody> bodies;
    std::vector<CatalogCompositionSource> sourceKinds;
};

class DeepSkyCatalogMerger final {
public:
    [[nodiscard]] static DeepSkyCatalogMergeResult merge(
        std::span<const CelestialBody> activeBodies,
        std::span<const CatalogCompositionSource> activeSourceKinds,
        std::span<const CelestialBody> deepSkyBodies
    );
};

}  // namespace skygate::ephemeris
