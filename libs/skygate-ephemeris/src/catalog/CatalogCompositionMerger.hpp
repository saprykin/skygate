#pragma once

#include "catalog/CatalogCompositionMergeResult.hpp"
#include "catalog/CatalogCompositionRequest.hpp"

namespace skygate::ephemeris {

// Identity-based merge for catalog source collections.
//
// This is the single implementation of the composition merge policy. Within a
// source, the first record for an identity is authoritative and later
// duplicate records fill its missing metadata. Across replacing sources, a
// later source has higher precedence: its matching record replaces the earlier
// source's survivor and absorbs the earlier body's non-conflicting
// identifiers, aliases, and metadata. A record of the currently merged source
// that resolves to a survivor this same source pass already produced fills
// that survivor's missing metadata instead of replacing it. AugmentCore
// sources contribute only non-deep-sky bodies as a gap-fill and enable the
// bundled bright-star fallback when no star is present.
//
// The identity-to-survivor index stays current while the collection is merged:
// a replacement vacates the earlier survivor's index entries and registers the
// replacement, and identifiers acquired by a metadata union are registered
// before the next record resolves. Later records can therefore match
// identifiers an earlier record acquired in the same source pass.
//
// A record whose authoritative identifiers (external identifiers and
// recognized deep-sky designations) match several survivors of the same kind
// bridges them: the bridging record becomes the single survivor and absorbs
// every matched survivor's identity, metadata, and contributors. Matched
// survivors of an incompatible kind stay distinct with a diagnostic, as do
// weak deep-sky alias matches that contradict recognized designations or match
// several survivors.
//
// After the merge, an authoritative identity shared by two active survivors of
// the same kind is an internal merge error and is reported by the merge
// validation. A shared authoritative identity across incompatible kinds is a
// deliberate, already diagnosed conflict and stays visible as such.
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
