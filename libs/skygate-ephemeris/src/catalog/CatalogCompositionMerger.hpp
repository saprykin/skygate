#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/CatalogCompositionMergeResult.hpp"
#include "catalog/CatalogCompositionRequest.hpp"
#include "catalog/CatalogCompositionSource.hpp"
#include "catalog/composition/DeepSkyCatalogMergeResult.hpp"

#include <span>
#include <string_view>

namespace skygate::ephemeris {

// Identity-based merge for catalog source collections.
//
// This is the single implementation of the composition merge policy. Within a
// source, the first record for an identity is authoritative and later
// duplicate records fill its missing metadata. Across replacing sources, a
// later source has higher precedence: its matching body replaces the earlier
// body and absorbs the earlier body's non-conflicting identifiers, aliases,
// and metadata. AugmentCore sources contribute only non-deep-sky bodies as a
// gap-fill and enable the bundled bright-star fallback when no star is present.
class CatalogCompositionMerger final {
public:
    [[nodiscard]] static CatalogCompositionMergeResult mergeCollection(const CatalogCompositionRequest& request);

    // Legacy two-slot merge retained for DeepSkyCatalogMerger compatibility.
    [[nodiscard]] static DeepSkyCatalogMergeResult mergeTwoSlot(
        std::span<const BaseCelestialBody* const> activeBodies,
        std::span<const CatalogCompositionSource> activeSourceKinds,
        std::span<const BaseCelestialBody* const> deepSkyBodies
    );

    // Stable source-ID spelling for the legacy composition source categories.
    [[nodiscard]] static std::string_view sourceKindId(CatalogCompositionSource source) noexcept;
    [[nodiscard]] static CatalogCompositionSource sourceKindFromId(std::string_view sourceId) noexcept;
};

}  // namespace skygate::ephemeris
