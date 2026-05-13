## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-026B
- Title: Implement Earth rotation and terrestrial transforms
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: d9c3d76
- Head ref: 2b08dff

## Summary

The implementation adds CIRS/TIRS/ITRS stages through `ErfaFrameTransformer`, wraps ERFA Earth rotation and polar-motion helpers, samples EOP data for polar motion, and adds SOFA-style reference tests for the new frame stages. The core transform approach looks aligned with the task, but the required degradation test coverage is incomplete: stale EOP metadata is not tested even though HP-026B explicitly requires it.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Stale EOP degradation metadata is not tested

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`
Lines/functions: `FrameTransformerTests`, missing test near existing EOP degradation tests

Problem:
HP-026B verification requires tests that missing, stale, predicted, or estimated EOP data produces degraded transform metadata. The new test coverage includes predicted EOP data in `degradesItrsTransformForPredictedEarthOrientationData()` and missing EOP data in `degradesItrsTransformForMissingEarthOrientationData()`, but it does not include a stale EOP data case. The test helper already accepts an `EarthOrientationDataStatus` argument, so this appears to be an omitted test case rather than a hard-to-cover path.

Why it matters:
Stale EOP data is a distinct degradation mode surfaced by `EarthOrientationProvider`. Without a frame-transformer test for it, a regression in the EOP-to-transform metadata mapping could still satisfy the current HP-026B tests while failing the task's acceptance criteria.

Recommended fix:
Add a frame-transformer test using an `EarthOrientationDataStatus::Stale` provider, transform through the TIRS-to-ITRS stage, and assert the result vector is present, metadata status is `Degraded`, and `AccuracyDegraded` is reported. If the project adds a separate estimated-EOP model later, add the analogous coverage for that state as well; currently the EOP sample model exposes stale, predicted, missing, out-of-range, and invalid-input warnings, but no explicit estimated warning.

## Test assessment

The implementation adds SOFA/ERFA-reference tests for CIRS-to-TIRS, TIRS-to-ITRS, and GCRS-to-ITRS transforms, plus degradation tests for predicted and missing EOP data. The isolated stale EOP degradation case required by HP-026B is missing.

I ran `ctest --test-dir build-ralph --output-on-failure`; the available simple-only suite passed 58/58 tests. The HP-026B-specific `skygate-ephemeris-frame-transformer-tests` target could not be run from the existing `build-ralph` tree because it is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so that target is not generated.

## Regression risk

Medium

The transform code touches shared frame transformation behavior and metadata for terrestrial stages. The math is covered by new reference tests in source, but the high-precision tests were not runnable in the current build configuration and one required degradation mode lacks direct coverage.

## Out-of-scope observations

- HP-026B mentions estimated EOP degradation, but the current `EarthOrientationSampleWarningCode` model does not expose an explicit estimated-data warning. This may need clarification in a future API/modeling task.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
