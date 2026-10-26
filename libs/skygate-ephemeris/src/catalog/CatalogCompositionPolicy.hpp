#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Merge behavior applied to one catalog source during active composition.
//
// A source collection composes in order. `Merge` and `DeepSkyOnly` sources are
// replacing: a later source has higher precedence and its matching body
// replaces the earlier body while absorbing the earlier body's non-conflicting
// metadata. `AugmentCore` is the bundled-core augmentation: it contributes
// only non-deep-sky bodies as a gap-fill and enables the bundled bright-star
// fallback when no other source contributes a star. A gap-fill body is skipped
// only when the shared identity decision resolves it to an existing survivor
// through the same kind, designation, and ambiguity checks as a replacing
// source; a contradicted or ambiguous alias match stays a distinct object.
// `DeepSkyFallback` is the bundled deep-sky fallback: it contributes only
// deep-sky bodies as a gap-fill, so it never replaces a configured source's
// body or precedence. Bundled augmentation and fallback carry their own
// provenance instead of being attributed to another source.
enum class CatalogCompositionPolicy : std::uint8_t {
    Merge,
    DeepSkyOnly,
    AugmentCore,
    DeepSkyFallback
};

}  // namespace skygate::ephemeris
