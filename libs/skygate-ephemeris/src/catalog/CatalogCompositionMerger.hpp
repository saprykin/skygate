#pragma once

#include "catalog/CatalogCompositionMergeResult.hpp"
#include "catalog/CatalogCompositionRequest.hpp"

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
//
// A canonical id whose CatalogObjectIdentity::idScope is SourceLocal is a
// record key of one source instance only. The merge qualifies it with the
// owning source instance id ("hyg_auto_1@<sourceId>") before identity
// resolution, so equal generated counters from unrelated sources never match
// while a reload of the same instance keeps the same object key.
//
// A deep-sky alias match is weak evidence: it merges only when neither record
// carries a conflicting authoritative designation. Explicit external
// identifiers and recognized deep-sky canonical ids (ngc_<n>, ic_<n>,
// messier_<nnn>) both count as authoritative, so two distinct recognized
// designations that share a common name stay distinct instead of one being
// dropped.
class CatalogCompositionMerger final {
public:
    [[nodiscard]] static CatalogCompositionMergeResult mergeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
