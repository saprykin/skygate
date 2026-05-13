## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027D
- Title: Integrate precession and nutation into apparent RA/Dec
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 8d4b5c0
- Head ref: 7bfe184

## Summary

The implementation changes apparent-place routing so the precession/nutation flag selects the GCRS-to-CIRS path, and adds tests for enabled/disabled routing, transformed RA/Dec output, and mocked metadata propagation. The functional routing change is focused and consistent with the HP-027D scope, but one verification requirement is not fully covered: degraded time/EOP input propagation is only tested with synthetic transformer metadata, not through the real frame-transform path.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Required degraded-input propagation test is missing

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
Lines/functions: `propagatesDegradedTransformMetadataForPrecessionNutation`

Problem:
HP-027D verification requires tests that degraded time or EOP inputs propagate transform warnings into apparent-place metadata. The added test injects an already-degraded `EphemerisResultMetadata` into `RecordingFrameTransformer`, then verifies `ApparentPlaceCalculator` merges it. That covers metadata merging, but it does not exercise degraded time/EOP inputs through `ErfaFrameTransformer` and the apparent-place boundary.

Why it matters:
This task wires apparent RA/Dec through the frame transformer. A mock-only test can still pass if real time-scale degradation, EOP degradation, or transformer warning construction stops reaching apparent-place results. The claimed acceptance criterion is therefore only partially verified.

Recommended fix:
Add an integration-style apparent-place test that constructs `ApparentPlaceCalculator` with a real `ErfaFrameTransformer` and controlled degraded time-scale or EOP provider input, requests `PrecessionNutation`, and asserts the resulting apparent-place metadata has the expected degraded status and warnings.

## Test assessment

The added tests cover precession/nutation-enabled CIRS routing, no-precession GCRS routing, use of the transformer's output vector for RA/Dec, and merge behavior for degraded transformer metadata. `ctest --test-dir build-ralph --output-on-failure` passed 59/59 tests.

I also attempted `cmake --build build-ralph --target skygate-ephemeris-highprecision-tests`, but that aggregate target does not exist in this build tree. The directly relevant CTest target `skygate-ephemeris-apparent-place-calculator-tests` is present and passed as part of the full CTest run.

Missing coverage: degraded time/EOP inputs flowing through a real frame transformer into `ApparentPlaceCalculator`.

## Regression risk

Low

The production change is a narrow flag-routing fix and the existing test suite passes. The remaining risk is mostly in unverified degraded metadata propagation at the apparent-place integration boundary.

## Out-of-scope observations

- `ApparentPlaceCalculator::mergeMetadata` only promotes `Failed` and `Degraded` statuses from transformer metadata. If frame transforms later return `OutOfRange` or `Unsupported` with a vector, those statuses would not propagate. This was not introduced by HP-027D but should be covered in a future metadata-hardening task.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
