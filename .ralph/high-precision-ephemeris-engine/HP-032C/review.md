## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-032C
- Title: Integrate batch path into full-frame computation
- Source: IMPLEMENTATION_PLAN.md
- Base ref: c091ec5a485d1a66c0dc4c43060b73ffa8c4bfa0
- Head ref: 93bc3c3bef818b4bc34a8eb4951d09dc334176d5

## Summary

The implementation wires full-frame high-precision snapshots through the star
batch calculator and preserves single-object requests on the existing
single-body path. However, the concrete batch calculator still performs the
expensive per-request time-scale, kernel, and apparent-place work per star, and
the new tests do not include the requested timing or allocation comparison.
That leaves a core HP-032C performance requirement unmet.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Real batch path still repeats per-request work per star

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: `StarAstrometryCalculator::calculateBatch`, lines 488-505

Problem:
`calculateBatch()` reserves one result vector, but then loops through every
catalog star and calls `calculateStarAstrometry()` for each item. For requests
with annual parallax, that helper repeats TDB conversion and Earth kernel
lookup for every star. Afterward, `HighPrecisionEphemerisEngine::compute()`
applies apparent-place processing once per batch result, which repeats
time-scale, EOP, and frame-transform work per star as well.

Why it matters:
HP-032C explicitly requires applying shared transform state once per
frame/request for the full-frame catalog-star path. The current integration
uses the batch entry point, but it does not yet satisfy the shared-state part of
the task, so large catalog frames can retain the same expensive per-star
overhead the task is intended to remove.

Recommended fix:
Move request-wide work into a shared per-frame context for the batch path. At a
minimum, compute reusable time-scale conversions, Earth kernel state, EOP
samples, and frame transforms once per request where the requested corrections
allow it, then pass that state through the batch star and apparent-place
pipeline. Add verification that the shared collaborators are called once per
snapshot rather than once per star.

### Finding 2: Performance acceptance is not verified by a benchmark

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
Lines/functions: `batchesRepresentativeLargeCatalogWithoutSingleStarDispatch`,
lines 837-861

Problem:
The representative large-catalog test uses a fake batch calculator and verifies
one batch call plus zero single-star fallback calls. It does not measure timing,
allocation counts, or collaborator call counts against a simple per-object
baseline.

Why it matters:
HP-032C asks for large-catalog performance tests or benchmarks with a
documented representative catalog size, and for timing or allocation comparison
against the simple per-object baseline. The current test proves dispatch shape,
but it would still pass if the real batch implementation did the same
per-object work internally.

Recommended fix:
Add a focused performance guard or benchmark in the existing test style using a
documented representative catalog size. Compare the integrated batch path
against a per-object baseline, or instrument allocation/collaborator call
counts so the test fails if request-wide transform work is repeated per star.

## Test assessment

Relevant tests were added for full-frame batch dispatch, large-catalog dispatch
shape, stale snapshot inputs, and fallback when the default batch method returns
no results. The suite passes, but coverage is too shallow for HP-032C's
performance acceptance criteria because it does not exercise the real shared
transform state or benchmark the real batch implementation against a
per-object baseline.

Commands run:

- `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision`
  `--output-on-failure`
- `ctest --test-dir build-ralph -R skygate-ephemeris --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

All 123 configured tests passed in the full suite. The two CALCEPH-disabled
tests were skipped by configuration.

## Regression risk

Medium

The implementation is localized and existing tests pass, but the remaining
per-star request-wide work is exactly in the full-frame performance path that
HP-032C is meant to improve.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
