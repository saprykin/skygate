#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

namespace skygate::ephemeris {

// How a composition policy classifies records. The merger selects records with
// `participates` and the composer counts the deep-sky contribution of a source
// with `contributesDeepSkyObjects`. The merger also picks the merge mode with
// `isGapFill` and decides the bundled bright-star fallback with
// `enablesBundledStarFallback`. Every consumer shares these predicates so a new
// policy declares its rules in one place instead of leaving the merge and the
// counts to drift apart.
class CatalogCompositionPolicyRules final {
public:
    // True when a policy value names a policy this build knows, so persisted
    // values are checked before they reach composition.
    [[nodiscard]] static bool isKnown(CatalogCompositionPolicy policy) noexcept;
    [[nodiscard]] static bool participates(CatalogCompositionPolicy policy, BaseCelestialBody::Kind kind) noexcept;
    [[nodiscard]] static bool participates(CatalogCompositionPolicy policy, const BaseCelestialBody& body) noexcept;
    // True when a source of this policy contributes deep-sky bodies to a
    // composition, the rule behind the composer's found-object count.
    [[nodiscard]] static bool contributesDeepSkyObjects(CatalogCompositionPolicy policy) noexcept;
    // True when a policy contributes its bodies as a gap fill: a body whose
    // shared identity already resolves to a survivor is preserved instead of
    // replacing it, so the source never overrides an earlier position.
    [[nodiscard]] static bool isGapFill(CatalogCompositionPolicy policy) noexcept;
    // True when a policy also enables the bundled bright-star fallback, which
    // fills a composition that no source supplied a star for.
    [[nodiscard]] static bool enablesBundledStarFallback(CatalogCompositionPolicy policy) noexcept;
};

}  // namespace skygate::ephemeris
