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
// that survivor's missing fields, keeps the values it inherited from other
// sources, and resolves a value both records supply from this source by the
// earlier supplying row, because bridge absorption can move that record behind
// rows it outranks. AugmentCore sources contribute only non-deep-sky bodies
// as a gap-fill and enable the bundled bright-star fallback when no star is
// present; DeepSkyFallback sources contribute only deep-sky bodies as a
// gap-fill. Gap-fill sources never replace an earlier survivor, so a
// configured source's body always keeps precedence over bundled fallback
// data. Every policy shares the same identity decision: a gap-fill body is
// only skipped when its identity resolves to an existing survivor through the
// same kind, designation, and ambiguity checks as a replacing source, and a
// contradicted or ambiguous weak alias match keeps the body as an independent
// object with the normal diagnostic.
//
// The identity-to-survivor index stays current while the collection is merged:
// a replacement vacates the earlier survivor's index entries and registers the
// replacement, and identifiers acquired by a metadata union are registered
// before the next record resolves. Later records can therefore match
// identifiers an earlier record acquired in the same source pass.
//
// A replacement or bridge never forgets an authoritative canonical
// equivalence. The absorbed body's canonical id, and every canonical id it
// already retained, stay on the survivor as
// CatalogObjectIdentity::retainedCanonicalIds. The chosen public canonical id
// (BaseCelestialBody::id) stays the winner's own, while an earlier id still
// resolves to the survivor through later rows, later compositions, restored
// snapshots, and a snapshot fed back as a composition input. Retained keys are
// authoritative identity data: they resolve like canonical ids, set the same
// ambiguity rules, and are never demoted to weak display aliases.
//
// A record whose authoritative identifiers (external identifiers and
// recognized deep-sky designations) match several survivors of the same kind
// bridges them: the bridging record becomes the single survivor and absorbs
// every matched survivor's identity, metadata, and contributors. The bridging
// record supplies the public canonical id, while the metadata of the earlier
// rows stays authoritative: identity bridging decides which records are one
// object, not which row outranks which. Matched survivors of an incompatible
// kind stay distinct with a diagnostic, as do weak deep-sky alias matches that
// contradict recognized designations or match several survivors.
//
// Absorption follows configured source precedence, never accumulator
// insertion order, and it applies per field: every enriched field, including
// the visual magnitude, keeps the origin of the source and row that actually
// supplied its value, so an intermediate survivor never promotes a value it
// inherited to its own, higher rank. A value is therefore taken from the
// highest-precedence source that supplied it, whether the winner supplied it
// itself or absorbed it, and only that value replaces a lower-precedence one.
// Values supplied by one source resolve by row order: the earliest row that
// supplied a field keeps it, so a bridge across survivors of one source
// cannot outrank the rows that supplied their metadata and later rows only
// fill missing fields. Absent fields stay absent and a valid zero is a value
// like any other. A survivor's contributor source ids are listed in that same
// descending precedence order: the winner's source first, then each remaining
// contributor highest first, with every contributing source listed once. A
// record of the currently merged source that resolves to a survivor this same
// source pass already produced follows the same rule: it fills missing
// fields, keeps values the survivor inherited from other sources, and cannot
// replace a value supplied by an earlier row of this source, even when a
// bridge appends that survivor later. Accumulator positions are not source
// ranks and not row order: replacement and bridge absorption vacate positions
// and append the winner at a new one, so position order stops matching
// precedence after the first replacement.
//
// Field origins are part of one merge run and are not carried by a body: an
// already composed snapshot enters a later composition as one source, so every
// value it holds counts as supplied by that source and earlier field history
// is not recovered.
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
//
// Coordinates are merged as one coherent model per survivor. The astrometry
// reference position and reference epoch are authoritative, and the fixed
// position whose value origin outranks the others wins; fixed positions of one
// source resolve by row order like every other value. A losing
// fixed position therefore fills a missing winner position and replaces a
// position the winner only inherited from a lower-precedence source, and it
// does either only when it agrees with the surviving model, because a fixed
// position carries no reference epoch to convert; a losing astrometry only
// enters the record when its reference position agrees with the surviving
// model, either directly against a fixed position or against the winning
// reference position after the losing proper motion accounts for the
// reference-epoch difference. Compatible losing astrometry fills missing
// proper motion, parallax, radial velocity, and validity fields and replaces a
// field the winner only inherited from a lower-precedence source, while
// a value the winner carries from a higher-precedence source than the
// absorbed contributor is never overwritten. Incompatible losing coordinates are rejected with a diagnostic that
// names the kept model, so a survivor never combines contradictory coordinate
// descriptions.
class CatalogCompositionMerger final {
public:
    [[nodiscard]] static CatalogCompositionMergeResult mergeCollection(const CatalogCompositionRequest& request);
};

}  // namespace skygate::ephemeris
