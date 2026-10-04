#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Merge behavior applied to one catalog source during active composition.
//
// A source collection composes in order. `Merge` and `DeepSkyOnly` sources are
// replacing: a later source has higher precedence and its matching body
// replaces the earlier body while absorbing the earlier body's non-conflicting
// metadata. `AugmentCore` is the bundled-core augmentation: it contributes
// only non-deep-sky bodies as a gap-fill (existing identities are kept) and
// enables the bundled bright-star fallback when no other source contributes a
// star. Bundled augmentation carries its own provenance instead of being
// attributed to the primary source.
enum class CatalogCompositionPolicy : std::uint8_t {
    Merge,
    DeepSkyOnly,
    AugmentCore
};

}  // namespace skygate::ephemeris
