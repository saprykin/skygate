#pragma once

#include "BaseCelestialBody.hpp"
#include "DeepSkyCatalogMergeResult.hpp"

#include <span>

namespace skygate::ephemeris {

class DeepSkyCatalogMerger final {
public:
    [[nodiscard]] static DeepSkyCatalogMergeResult merge(
        std::span<const BaseCelestialBody* const> activeBodies,
        std::span<const CatalogCompositionSource> activeSourceKinds,
        std::span<const BaseCelestialBody* const> deepSkyBodies
    );
};

}  // namespace skygate::ephemeris
