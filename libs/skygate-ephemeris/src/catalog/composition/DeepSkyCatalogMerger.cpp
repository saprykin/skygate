#include "DeepSkyCatalogMerger.hpp"

#include "catalog/CatalogCompositionMerger.hpp"

namespace skygate::ephemeris {

DeepSkyCatalogMergeResult DeepSkyCatalogMerger::merge(
    const std::span<const BaseCelestialBody* const> activeBodies,
    const std::span<const CatalogCompositionSource> activeSourceKinds,
    const std::span<const BaseCelestialBody* const> deepSkyBodies
)
{
    return CatalogCompositionMerger::mergeTwoSlot(activeBodies, activeSourceKinds, deepSkyBodies);
}

}  // namespace skygate::ephemeris
