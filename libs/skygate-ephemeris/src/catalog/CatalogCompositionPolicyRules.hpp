#pragma once

#include "BaseCelestialBody.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"

namespace skygate::ephemeris {

// How a composition policy classifies records. The merger selects records with
// `participates` and the composer counts the deep-sky contribution of a source
// with `contributesDeepSkyObjects`; both consumers share these predicates so a
// new policy declares its selection rule in one place instead of leaving the
// counts to drift from the merge.
class CatalogCompositionPolicyRules final {
public:
    [[nodiscard]] static bool participates(CatalogCompositionPolicy policy, BaseCelestialBody::Kind kind) noexcept;
    [[nodiscard]] static bool participates(CatalogCompositionPolicy policy, const BaseCelestialBody& body) noexcept;
    // True when a source of this policy contributes deep-sky bodies to a
    // composition, the rule behind the composer's found-object count.
    [[nodiscard]] static bool contributesDeepSkyObjects(CatalogCompositionPolicy policy) noexcept;
};

}  // namespace skygate::ephemeris
