## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-032B
- Title: Add batch star astrometry propagation
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 6ad5ef75b08068f380947eaec33a2addce9609fd
- Head ref: 260db204580a3c1501130c1ddf92dbdf9cce85c4

## Summary

The implementation adds `StarAstrometryCalculator::calculateBatch()` over
`CatalogStarAstrometryArrays` and focused batch parity tests. The change is
mostly scoped to HP-032B and the available test suite passes. However, the
batch path rebuilds scalar astrometry from sanitized arrays, which can change
error handling for non-finite optional astrometry values compared with
single-star propagation. One explicit batch-test acceptance point is also not
directly covered.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Batch can mask invalid optional astrometry

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: `astrometryFromArrays()`, lines 460-475

Problem:
`calculateBatch()` reconstructs `CatalogStarAstrometry` through
`CatalogStarAstrometryArrays` accessors. Those arrays mark non-finite optional
proper-motion and radial-velocity fields as absent when they are built, and the
accessors then return `std::nullopt`. The scalar calculator sees a present
NaN/Inf optional field, `finiteValueOrZero()` returns `std::nullopt`, and
`propagatedAstrometricVector()` fails the computation. The batch path instead
turns the same bad field into a missing value and can return a degraded result.

Why it matters:
HP-032B requires batch results to stay numerically consistent with
`StarAstrometryCalculator` single-star outputs and to respect the same fallback
semantics. Catalog rows with invalid optional astrometry currently produce
different statuses between scalar and batch computation.

Recommended fix:
Preserve enough validity state in `CatalogStarAstrometryArrays` to distinguish
missing optional fields from present non-finite fields, or make scalar and array
construction share the same normalization policy. Add a batch parity test with
present NaN/Inf optional astrometry fields so the expected status is locked
down.

### Finding 2: Disabled correction flags are not batch-tested

Severity: MINOR
File: `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
Lines/functions: `batchMatchesSingleStarPropagationForFullPartialAndFixedStars()`,
`batchMatchesSingleStarAnnualParallaxCorrections()`

Problem:
HP-032B verification asks for correction flags enabled and disabled. The new
batch tests cover enabled proper-motion/parallax/radial-velocity flags and
annual parallax, but no `calculateBatch()` test uses
`EphemerisCorrectionFlags::NoCorrections`.

Why it matters:
The implementation handoff claims correction flags enabled/disabled are covered
by tests, but the disabled case is only covered for the scalar path. A future
batch-specific implementation could accidentally apply corrections when flags
are disabled without failing the current batch tests.

Recommended fix:
Add a batch parity test with `EphemerisCorrectionFlags::NoCorrections` covering
full-astrometry, partial-astrometry, and fixed-only stars. Assert coordinates,
status, and applied/unavailable correction metadata match scalar outputs.

## Test assessment

The focused star astrometry test target includes new batch parity checks for
full, partial, fixed-only, and annual-parallax cases. The tests are registered
and runnable. Missing coverage is the disabled correction-flag batch case and a
batch parity case for present non-finite optional astrometry fields.

Commands run:

- `git diff --check HEAD^ HEAD`
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-star-astrometry-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure`

The full available suite passed 123/123 tests. The CALCEPH kernel-provider and
solar-system calculator tests were skipped by the existing build configuration.

## Regression risk

Medium

The changed code is localized to the star astrometry calculator, but the new
batch path is intended to become the catalog-star full-frame path. Divergent
fallback/error semantics would be difficult to diagnose once HP-032C integrates
the batch results into snapshots.

## Out-of-scope observations

- The batch method currently lives only on the concrete
  `StarAstrometryCalculator`, while the engine dependency uses
  `IStarAstrometryCalculator`. HP-032C may need to expose a batch-capable
  interface or otherwise avoid downcasting when integrating full-frame
  computation.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
