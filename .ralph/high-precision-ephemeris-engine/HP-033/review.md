## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-033
- Title: Add high-precision computation cache and read-only thread safety
- Source: IMPLEMENTATION_PLAN.md, specs/high-precision-ephemeris-engine.md
- Base ref: 39ff3f28554737b23e314b3a7c21252d4a4ba65d
- Head ref: 9b64342b0ac6d327abe516e11e3f096d61b058e3

## Summary

The implementation adds an `EphemerisComputationCache`, wires a default cache
into factory-created high-precision engines, and adds tests for repeated
snapshot reads, warmed-cache concurrent reads, and basic dataset-version cache
isolation. The code is thread-safe for the cache map itself and the full suite
passes, but the implementation is a whole-snapshot memoization layer rather
than the shared per-request computation cache required by HP-033. The cache key
also does not reliably represent catalog or full dataset identity, so the
claimed isolation guarantees are weaker than the task requires.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Cache stores snapshots, not shared per-request state

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
Lines/functions: lines 48-78, `IEphemerisComputationCache`

Problem:
HP-033 requires `EphemerisComputationCache` to hold shared per-request state
such as time-scale conversions, Earth state, observer geocentric position,
Earth-orientation matrices, precession/nutation matrices, apparent-place
constants, and atmospheric constants. The cache interface only exposes
`findSnapshot()` and `storeSnapshot()` for complete `SkySnapshot` objects.
`HighPrecisionEphemerisEngine::compute()` only uses that full-snapshot cache,
and `computeBodyState()` only reads an already cached full-frame snapshot for
the index overload.

Why it matters:
This does not satisfy the core task requirement. It avoids recomputation only
after an identical full-frame request has already completed, but it does not
centralize or reuse the expensive request-scoped time/frame/observer constants
within a first full-frame computation or across single-body computations.

Recommended fix:
Introduce an immutable request-preparation/cache entry that stores the shared
time, Earth-orientation, frame-transform, observer, apparent-place, and
atmospheric state required by the calculators. Thread that prepared state
through full-frame and single-body paths, while keeping whole-snapshot caching
as an optional additional optimization if still useful.

### Finding 2: Catalog isolation is based on pointer identity and size

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.cpp`
Lines/functions: line 112, `EphemerisComputationCache::makeSnapshotKey`

Problem:
The snapshot key identifies the catalog as `catalogBodies.data()` plus
`catalogBodies.size()`. That is storage identity, not catalog identity or
content. If a shared cache outlives an engine/catalog instance, or if different
catalog contents reuse the same vector storage address and size, the cache can
return a stale snapshot for the wrong catalog.

Why it matters:
The implementation handoff claims cached results are isolated by catalog, but
the key does not encode a catalog revision or stable content fingerprint. A
stale catalog snapshot would attach states to the wrong bodies while still
passing the current single-body cache tests.

Recommended fix:
Key cache entries with an explicit immutable catalog revision/snapshot identity
from the catalog/runtime layer, or compute and store a stable catalog snapshot
token when the engine copies the catalog bodies. Avoid using raw vector storage
addresses as a correctness boundary.

### Finding 3: Dataset isolation omits effective range contents

Severity: MINOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.cpp`
Lines/functions: lines 113-114, `EphemerisComputationCache::makeSnapshotKey`

Problem:
The dataset portion of the key includes `id`, `version`, `provenance`, and only
the number of date ranges. It does not include the actual date-range identities
or start/end epochs. Two manifests with the same version/provenance and range
count but different effective validity ranges would collide.

Why it matters:
HP-033 asks for data-snapshot/revision isolation. If an active data snapshot is
rebuilt with corrected coverage metadata but unchanged coarse labels, stale
results can be reused with the wrong validity/provenance assumptions.

Recommended fix:
Use an explicit ephemeris data revision token or active data snapshot identity
in the key. If `EphemerisDataSetInfo` remains the key source, include all fields
that affect computation or result metadata, including date-range ids and epoch
bounds.

## Test assessment

Added tests cover repeated full-frame snapshot memoization for a one-body frame,
warmed-cache concurrent reads, and direct cache separation for two different
dataset versions. They do not cover the required shared per-request computation
state, cold concurrent computations, multi-body full-frame cache reuse, catalog
revision/content isolation, or old/new active data snapshot isolation through
real engine instances.

Relevant verification run:

- `ctest --test-dir build-ralph --output-on-failure -R '^skygate-ephemeris-highprecision-engine-tests$'`
  passed.
- `ctest --test-dir build-ralph --output-on-failure` passed: 124/124 tests
  passed; CALCEPH-dependent tests 33 and 34 were skipped by existing test
  conditions.

## Regression risk

Medium

The current implementation is mostly additive, but stale cache hits can produce
incorrect full-frame results if cache sharing expands or storage identity is
reused. The larger risk is that HP-033's intended request-scoped state reuse is
not actually in place, so later performance/concurrency work may build on a
cache abstraction that does not expose the state those paths need.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
