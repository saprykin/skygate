## Task fixed
  - ID: HP-027D
  - Title: Integrate precession and nutation into apparent RA/Dec
  - Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-027D/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-027D/implementation.md

## Summary
  Added the missing apparent-place integration coverage for degraded frame-transform metadata. The new test uses a real `ErfaFrameTransformer` with a degraded TT conversion service and verifies the apparent-place result receives degraded status, time-scale warnings, and applied precession/nutation metadata.

## Findings addressed
  - Finding title: Required degraded-input propagation test is missing
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp
  - What changed: Added `DegradedTtTimeScaleService` and `propagatesDegradedRealFrameTransformMetadataForPrecessionNutation()`.
  - Why this resolves the finding: The test now crosses the apparent-place boundary through a real `ErfaFrameTransformer` instead of injecting already-degraded metadata through the mock transformer.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests` - PASS
  - `ctest --test-dir build-ralph -R '^skygate-ephemeris-apparent-place-calculator-tests$' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS

## Files changed
  - libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-027D/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-027D/fix.md

## Remaining concerns
  `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so the real ERFA branch in the new test is skipped locally and will execute in high-precision-enabled builds.

## Final fixer status
  READY_FOR_REVIEW
